import hashlib
from pathlib import Path


def apply(root, destination):
    header=root/'optee_standalone/loader/signed_loader_audit.h'
    target=destination/'core/kernel/signed_loader_audit.h';target.write_bytes(header.read_bytes())
    (destination/'core/kernel/signed_loader_types.h').write_bytes((root/'optee_standalone/loader/signed_loader_types.h').read_bytes())
    (destination/'core/pta/signed_loader_audit_pta.c').write_bytes((root/'optee_standalone/loader/signed_loader_audit_pta.c').read_bytes())
    sub=destination/'core/pta/sub.mk';sub.write_bytes(sub.read_bytes()+b'\nsrcs-$(CFG_TZ_SIGNED_AUDIT_PTA) += signed_loader_audit_pta.c\n')
    path=destination/'core/kernel/ree_fs_ta.c';before=path.read_bytes();text=before.decode()
    replacements=[('struct ree_fs_ta_handle {','#include "signed_loader_audit.h"\nstruct ree_fs_ta_handle {\n    struct signed_loader_entry *audit;'),
                  ('\thandle = calloc(1, sizeof(*handle));','struct signed_loader_entry *audit = signed_loader_begin(uuid);\n\n\thandle = calloc(1, sizeof(*handle));'),
                  ('/* Request TA from tee-supplicant */','handle->audit = audit;\n\t/* Request TA from tee-supplicant */'),
                  ('/* Make secure copy of signed header */','signed_loader_note(audit, 64, TEE_SUCCESS);\n\t/* Make secure copy of signed header */'),
                  ('/* Validate header signature */','signed_loader_note(audit, 2, TEE_SUCCESS);\n\t/* Validate header signature */'),
                  ('\n\tincr0_count++;\n','\n\tincr0_count++;\n\tsigned_loader_note(audit, 4, res);\n'),
                  ('res = check_digest(handle);','res = check_digest(handle);\n\t\tsigned_loader_note(handle->audit, 16, res);')]
    for old,new in replacements:
        if old.startswith('\thandle = calloc'):
            begin=text.index('static TEE_Result ree_fs_ta_open(')
            end=text.index('\nstatic ',begin+1)
            body=text[begin:end]
            if body.count(old)!=1:raise ValueError('Unsupported ZIP open function')
            text=text[:begin]+body.replace(old,new,1)+text[end:]
        else:
            if text.count(old)!=1:raise ValueError('Unsupported ZIP loader source for audit: '+old)
            text=text.replace(old,new,1)
    path.write_text(text)
    changes=[dict(path='core/kernel/ree_fs_ta.c',original_sha256=hashlib.sha256(before).hexdigest(),sha256=hashlib.sha256(path.read_bytes()).hexdigest()),dict(path='core/kernel/signed_loader_audit.h',original_sha256=None,sha256=hashlib.sha256(header.read_bytes()).hexdigest())]

    for name in ['core/kernel/signed_loader_types.h','core/pta/signed_loader_audit_pta.c']:
        changes.append(dict(path=name,original_sha256=None,sha256=hashlib.sha256((destination/name).read_bytes()).hexdigest()))
    return changes
