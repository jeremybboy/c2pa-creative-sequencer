# Viewer Test Mode certificate

`src/provenance/TestTrust.h` embeds the public **C2PA Conformance Test Root** used by
the Conformulator. It is loaded only by the explicit, session-only viewer Test Mode.
It is not an official production trust list, a private signing key, or a certificate
copied from the asset being validated.

Source: [contentauth/c2pa-conformance-tool, CertificateManager.svelte](https://github.com/contentauth/c2pa-conformance-tool/blob/bfdfaf4a6ffe221f288ba78047886983bf41f3be/src/lib/CertificateManager.svelte),
constant `TEST_ROOT_CERT_PEM`, checked 2026-10-30.
Copyright 2026 Content Authenticity Initiative, Apache License 2.0 (included here).

Certificate SHA-256 fingerprint:
`41:BD:E3:25:75:33:13:FC:3D:C6:32:F1:8C:2B:7F:AD:E7:29:B9:4A:64:29:FE:E5:24:E3:47:11:0C:34:6D:BA`.
Subject/issuer: `CN=C2PA Conformance Test Root, O=C2PA Test, C=US`.
Validity: 2026-02-08 to 2036-02-06.

Updates require explicit source/fingerprint review and SDK validation; the app does
not fetch replacement anchors at runtime or trust arbitrary asset certificates.

Graph label mappings use explicit [IPTC Digital Source Type](https://cv.iptc.org/newscodes/digitalsourcetype/)
declarations (IPTC NewsCodes, CC BY 4.0), not an audio detector or a complete
Conformulator Signals rubric implementation.
