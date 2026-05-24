set(OFX_SDK_DIR "/Library/Application Support/Blackmagic Design/DaVinci Resolve/Developer/OpenFX"
    CACHE PATH "Path to the DaVinci Resolve OpenFX SDK")

if(NOT EXISTS "${OFX_SDK_DIR}/Support/include/ofxsImageEffect.h")
    message(WARNING "DaVinci Resolve OpenFX SDK not found at ${OFX_SDK_DIR}; skipping OFX build")
    return()
endif()

set(OFX_SUPPORT_SOURCES
    ${OFX_SDK_DIR}/Support/Library/ofxsCore.cpp
    ${OFX_SDK_DIR}/Support/Library/ofxsImageEffect.cpp
    ${OFX_SDK_DIR}/Support/Library/ofxsInteract.cpp
    ${OFX_SDK_DIR}/Support/Library/ofxsLog.cpp
    ${OFX_SDK_DIR}/Support/Library/ofxsMultiThread.cpp
    ${OFX_SDK_DIR}/Support/Library/ofxsParams.cpp
    ${OFX_SDK_DIR}/Support/Library/ofxsProperty.cpp
    ${OFX_SDK_DIR}/Support/Library/ofxsPropertyValidation.cpp
)

add_library(ASCIIify_ofx MODULE
    ASCIIifyPlugin.cpp
    ${OFX_SUPPORT_SOURCES}
)

target_include_directories(ASCIIify_ofx PRIVATE
    ${OFX_SDK_DIR}/OpenFX-1.4/include
    ${OFX_SDK_DIR}/Support/include
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(ASCIIify_ofx PRIVATE asciiify_core)

target_compile_features(ASCIIify_ofx PRIVATE cxx_std_17)
target_compile_options(ASCIIify_ofx PRIVATE -fvisibility=hidden)

if(APPLE)
    set(OFX_BUNDLE_DIR "${CMAKE_BINARY_DIR}/ASCIIify.ofx.bundle/Contents/MacOS")
    set(OFX_RESOURCES_DIR "${CMAKE_BINARY_DIR}/ASCIIify.ofx.bundle/Contents/Resources")
    set_target_properties(ASCIIify_ofx PROPERTIES
        PREFIX ""
        SUFFIX ".ofx"
        OUTPUT_NAME "ASCIIify"
        LIBRARY_OUTPUT_DIRECTORY "${OFX_BUNDLE_DIR}"
    )
    target_link_options(ASCIIify_ofx PRIVATE -bundle)

    add_custom_command(TARGET ASCIIify_ofx POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OFX_RESOURCES_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/resources/AndaleMono.ttf"
            "${OFX_RESOURCES_DIR}/AndaleMono.ttf"
        COMMENT "Packaging ASCIIify.ofx.bundle"
    )
else()
    set_target_properties(ASCIIify_ofx PROPERTIES PREFIX "" SUFFIX ".ofx")
endif()

install(DIRECTORY "${CMAKE_BINARY_DIR}/ASCIIify.ofx.bundle"
        DESTINATION /Library/OFX/Plugins
        OPTIONAL)
