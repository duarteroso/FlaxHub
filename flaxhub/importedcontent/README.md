# Downloaded Design from Figma to Qt

Save the downloaded design from Figma to Qt here, including its CMakeLists.txt.

If this folder has a CMakeLists.txt, the top-level CMakeLists.txt adds it to the build with `add_subdirectory`. Its
targets are not part of `appFlaxHub` yet, since nothing links them into the app. To use them, link them into
`appFlaxHub` with `target_link_libraries` in the top-level CMakeLists.txt.
