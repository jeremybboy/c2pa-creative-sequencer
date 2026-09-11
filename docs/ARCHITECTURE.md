# Architecture

The system keeps four concerns separate: creative state, audio execution, provenance state, and C2PA serialization. JUCE owns the application shell and custom interface; a narrow adapter shields application code from Tracktion Engine; the provenance service will translate a stable internal provenance model into `c2pa-cpp` calls only at its boundary.

The initial dependency and component flow is shown in the repository overview diagram. This document will expand only when implementation makes an architectural claim real.
