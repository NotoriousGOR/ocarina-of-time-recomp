# Define the path to the entitlements file
set(ENTITLEMENTS_FILE ${CMAKE_SOURCE_DIR}/.github/macos/entitlements.plist)
set(MACOSX_BUNDLE_BUNDLE_NAME "Ocarina &amp; Majora")
set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.github.notoriousgor.ocarinaandmajora")
set(MACOSX_BUNDLE_BUNDLE_VERSION "1.0")
set(MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0")
set(MACOSX_BUNDLE_ICON_FILE "AppIcon.icns")
set(MACOSX_BUNDLE_EXECUTABLE_NAME "Ocarina &amp; Majora")

# Set bundle properties
set_target_properties(Zelda64Recompiled PROPERTIES
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME "Ocarina & Majora"
        MACOSX_BUNDLE_BUNDLE_NAME "${MACOSX_BUNDLE_BUNDLE_NAME}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "${MACOSX_BUNDLE_GUI_IDENTIFIER}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${MACOSX_BUNDLE_BUNDLE_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${MACOSX_BUNDLE_SHORT_VERSION_STRING}"
        MACOSX_BUNDLE_ICON_FILE "${MACOSX_BUNDLE_ICON_FILE}"
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_BINARY_DIR}/Info.plist
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "-"
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS ${ENTITLEMENTS_FILE}
)

# Create icon files for macOS bundle
set(ICON_SOURCE ${CMAKE_SOURCE_DIR}/icons/512.png)
set(ICONSET_DIR ${CMAKE_BINARY_DIR}/AppIcon.iconset)
set(ICNS_FILE ${CMAKE_BINARY_DIR}/resources/AppIcon.icns)

# Create iconset directory and add PNG file
add_custom_command(
        OUTPUT ${ICONSET_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${ICONSET_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy ${ICON_SOURCE} ${ICONSET_DIR}/icon_512x512.png
        COMMAND ${CMAKE_COMMAND} -E copy ${ICON_SOURCE} ${ICONSET_DIR}/icon_512x512@2x.png
        COMMAND touch ${ICONSET_DIR}
        COMMENT "Creating iconset directory and copying PNG file"
)

# Convert iconset to icns
add_custom_command(
        OUTPUT ${ICNS_FILE}
        DEPENDS ${ICONSET_DIR}
        COMMAND iconutil -c icns ${ICONSET_DIR} -o ${ICNS_FILE}
        COMMENT "Converting iconset to icns"
)

# Custom target to ensure icns creation
add_custom_target(create_icns ALL DEPENDS ${ICNS_FILE})

# Set source file properties for the resulting icns file
set_source_files_properties(${ICNS_FILE} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
)

# Add the icns file to the executable target
target_sources(Zelda64Recompiled PRIVATE ${ICNS_FILE})

# Ensure Zelda64Recompiled depends on create_icns
add_dependencies(Zelda64Recompiled create_icns)

# Configure Info.plist
configure_file(${CMAKE_SOURCE_DIR}/.github/macos/Info.plist.in ${CMAKE_BINARY_DIR}/Info.plist @ONLY)

# Install the app bundle
install(TARGETS Zelda64Recompiled BUNDLE DESTINATION .)

# Ensure the entitlements file exists
if(NOT EXISTS ${ENTITLEMENTS_FILE})
    message(FATAL_ERROR "Entitlements file not found at ${ENTITLEMENTS_FILE}")
endif()

# Post-build steps for macOS bundle
add_custom_command(TARGET Zelda64Recompiled POST_BUILD
    # Copy and fix frameworks first
    COMMAND ${CMAKE_COMMAND}
        "-D BUNDLE_EXECUTABLE=$<TARGET_FILE:Zelda64Recompiled>"
        "-D BUNDLE_FRAMEWORKS_DIR=$<TARGET_BUNDLE_DIR:Zelda64Recompiled>/Contents/Frameworks"
        -P ${CMAKE_SOURCE_DIR}/.github/macos/fixup_bundle.cmake
    # SDL2-compat loads SDL3 by its unversioned name at runtime.
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3> $<TARGET_BUNDLE_DIR:Zelda64Recompiled>/Contents/Frameworks/libSDL3.dylib

    # Copy all resources
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets ${CMAKE_BINARY_DIR}/temp_assets
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/temp_assets/scss
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/temp_assets $<TARGET_BUNDLE_DIR:Zelda64Recompiled>/Contents/Resources/assets
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/temp_assets

    # Copy controller database
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/recompcontrollerdb.txt $<TARGET_BUNDLE_DIR:Zelda64Recompiled>/Contents/Resources/

    # Set RPATH
    COMMAND install_name_tool -add_rpath "@executable_path/../Frameworks/" $<TARGET_FILE:Zelda64Recompiled>

    # Sign the bundle
    COMMAND codesign --verbose=4 --options=runtime --no-strict --sign - --entitlements ${ENTITLEMENTS_FILE} --deep --force $<TARGET_BUNDLE_DIR:Zelda64Recompiled>

    COMMENT "Performing post-build steps for macOS bundle"
    VERBATIM
)
