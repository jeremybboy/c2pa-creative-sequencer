include(FetchContent)

# Tracktion Engine v3.2.0 pins this JUCE revision. Keep the pair together until
# a dedicated dependency upgrade proves a newer combination.
FetchContent_Declare(JUCE
    URL https://github.com/juce-framework/JUCE/archive/19edd538429c93d277bf95b55aaa7e3eb545f951.tar.gz
    URL_HASH SHA256=5be3406ca3c7e4e757b1ca4d8e9083024563ce05a3a675597adadbcc8f334e82
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

FetchContent_MakeAvailable(JUCE)

# SOURCE_SUBDIR avoids Tracktion's top-level example build and its SSH JUCE
# submodule URL. The application supplies the exact JUCE target above.
FetchContent_Declare(tracktion_engine
    URL https://github.com/Tracktion/tracktion_engine/archive/0a5f4e6a5f53d09c89b414a44386a12df7fa1ec6.tar.gz
    URL_HASH SHA256=06536e8d5d68a22100bab0c8db59402638eded4b7c7decce19d71b511d587238
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR modules)

FetchContent_MakeAvailable(tracktion_engine)
