#include "backends.h"
static enum zdma_result transfer(struct dma_validation_backend *backend, uint64_t source, uint64_t destination)
{
    struct zdma_policy request = backend->channel;
    /* 検証済みの三つの専用buffer以外は転送させない。 */
    if ((source != backend->normal_source && source != backend->secure_buffer) ||
        (destination != backend->normal_destination && destination != backend->secure_buffer) ||
        source > ZDMA_ADDR_LIMIT - backend->size || destination > ZDMA_ADDR_LIMIT - backend->size) { return ZDMA_INVALID; }
    request.source_start = source;
    request.source_end = source + backend->size;
    request.destination_start = destination;
    request.destination_end = destination + backend->size;
    return zdma_copy(&request, source, destination, backend->size, &backend->last_completion);
}
static bool allowed_copy(void *context)
{
    struct dma_validation_backend *backend = context;
    return backend->prepare_normal(backend->memory_context) &&
        transfer(backend, backend->normal_source, backend->normal_destination) == ZDMA_COMPLETE &&
        backend->normal_copy_matches(backend->memory_context);
}
static bool seed_secure(void *context)
{
    struct dma_validation_backend *backend = context;
    return backend->seed_secure(backend->memory_context);
}
static bool secure_unchanged(void *context)
{
    struct dma_validation_backend *backend = context;
    return backend->secure_unchanged(backend->memory_context);
}
static bool destination_unchanged(void *context)
{
    struct dma_validation_backend *backend = context;
    return backend->read_destination_unchanged(backend->memory_context);
}
static enum access_result forbidden_copy(struct dma_validation_backend *backend, bool write)
{
    backend->last_violation_valid = false;
    if (xpu_probe_begin(&backend->protection) != PROTECTION_OK) { return ACCESS_INCONCLUSIVE; }
    enum zdma_result result = transfer(backend,
        write ? backend->normal_source : backend->secure_buffer,
        write ? backend->secure_buffer : backend->normal_destination);
    enum protection_result captured = xpu_probe_capture(&backend->protection, &backend->last_violation);
    if (result != ZDMA_COMPLETE && result != ZDMA_TRANSFER_ERROR) { return ACCESS_INCONCLUSIVE; }
    if (captured != PROTECTION_OK) { return ACCESS_INCONCLUSIVE; }
    uint32_t expected = XMPU_SECURITY_VIOLATION | (write ? XMPU_WRITE_VIOLATION : XMPU_READ_VIOLATION);
    backend->last_violation_valid = xpu_capture_matches(&backend->last_violation,
        backend->secure_buffer, backend->master_id, expected);
    if (backend->last_violation_valid) {
        uint32_t unrelated = backend->last_completion.isr & ZDMA_ERRORS & ~(write ? ZDMA_WRITE_ERROR : ZDMA_READ_ERROR);
        return unrelated == 0U ? ACCESS_DENIED : ACCESS_INCONCLUSIVE;
    }
    return result == ZDMA_COMPLETE ? ACCESS_SUCCEEDED : ACCESS_INCONCLUSIVE;
}
static enum access_result forbidden_read(void *context) { return forbidden_copy(context, false); }
static enum access_result forbidden_write(void *context) { return forbidden_copy(context, true); }
static bool violation_matches(void *context, bool write)
{
    struct dma_validation_backend *backend = context;
    uint32_t expected = XMPU_SECURITY_VIOLATION | (write ? XMPU_WRITE_VIOLATION : XMPU_READ_VIOLATION);
    return backend->last_violation_valid && xpu_capture_matches(&backend->last_violation,
        backend->secure_buffer, backend->master_id, expected);
}
enum protection_result zcu106_make_dma_test_ops(struct dma_validation_backend *backend, struct dma_test_ops *ops)
{
    if (backend == NULL || ops == NULL) { return PROTECTION_INVALID; }
    *ops = (struct dma_test_ops){ 0 };
    if (!backend->channel.route_verified || !backend->channel.nonsecure_attribute_verified ||
        !backend->channel.exclusive_channel_verified || !backend->channel.buffers_and_cache_verified ||
        !backend->protection.exclusive_verified) { return PROTECTION_BLOCKED; }
    if (backend->size == 0U || backend->size > HARDWARE_DMA_BYTES || backend->master_id > XMPU_MASTER_MASK ||
        backend->prepare_normal == NULL || backend->normal_copy_matches == NULL || backend->seed_secure == NULL ||
        backend->secure_unchanged == NULL || backend->read_destination_unchanged == NULL) { return PROTECTION_INVALID; }
    *ops = (struct dma_test_ops){ backend, true, allowed_copy, seed_secure, secure_unchanged,
        forbidden_read, destination_unchanged, forbidden_write, violation_matches };
    return PROTECTION_OK;
}
