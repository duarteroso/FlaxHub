cmake_minimum_required(VERSION 3.22)

project(FlaxHub VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick QuickDialogs2 Network QuickEffects Concurrent LinguistTools)

qt_standard_project_setup(REQUIRES 6.10)

# Warning flags for the app and its tests. MSVC (cl.exe, also under clang-cl) takes /W4 instead of the GCC and Clang flags.
if (MSVC)
    set(FLAXHUB_WARNING_FLAGS /W4)
else()
    set(FLAXHUB_WARNING_FLAGS -Wall -Wextra)
endif()

# Content of downloaded design from Figma to Qt or similar
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/importedcontent/CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/importedcontent)
endif()

qt_add_executable(appFlaxHub
    main.cpp
)

set_source_files_properties(Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(appFlaxHub
    URI FlaxHub
    QML_FILES
        Main.qml
        Theme.qml
        components/Sidebar.qml
        components/Icon.qml
        components/SidebarButton.qml
        components/FlaxEngineLink.qml
        components/ViewHeader.qml
        components/HubButton.qml
        components/HubDialog.qml
        components/ConfirmDialog.qml
        components/HubTextField.qml
        components/HubSearchField.qml
        components/HubCard.qml
        components/HubProgressBar.qml
        components/RowMenuButton.qml
        components/SettingsRow.qml
        components/SectionLabel.qml
        components/StatusBadge.qml
        components/EmptyState.qml
        components/InstallStep.qml
        components/FolderField.qml
        views/ProjectsView.qml
        views/InstallsView.qml
        views/SettingsView.qml
        dialogs/AddInstallDialog.qml
        dialogs/InstallProgressDialog.qml
        dialogs/NewProjectDialog.qml
        dialogs/NoticeDialog.qml
    RESOURCES
        icons/Logo_Title.png
        icons/joystick.svg
        icons/install_desktop.svg
        icons/open_in_new.svg
        icons/settings.svg
        icons/star.svg
        icons/star_filled.svg
        icons/sync_saved_locally.svg
    SOURCES
        src/appsettings.h
        src/appsettings.cpp
        src/enginecatalog.h
        src/enginecatalog.cpp
        src/engineregistry.h
        src/installrunner.h
        src/installrunner.cpp
        src/installlistmodel.h
        src/installlistmodel.cpp
        src/installtable.h
        src/installtable.cpp
        src/installstore.h
        src/installstore.cpp
        src/installsteps.h
        src/installsteps.cpp
        src/installfolderjobs.h
        src/installfolderjobs.cpp
        src/installlayout.h
        src/installrules.h
        src/installrules.cpp
        src/installvalidator.h
        src/installvalidator.cpp
        src/projectlistmodel.h
        src/projectlistmodel.cpp
        src/projectcreator.h
        src/projectcreator.cpp
        src/pathutils.h
        src/settingsarray.h
        src/versionutils.h
        src/filemanager.h
        src/filemanager.cpp
)

target_include_directories(appFlaxHub PRIVATE src)
target_compile_options(appFlaxHub PRIVATE ${FLAXHUB_WARNING_FLAGS})

# The English text of every id-based string. main.cpp loads flaxhub_en.qm from the embedded resources.
qt_add_translations(appFlaxHub
    TS_FILES
        translations/flaxhub_en.ts
)

set_target_properties(appFlaxHub PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appFlaxHub
    PRIVATE Qt6::Quick Qt6::QuickDialogs2 Qt6::Network Qt6::QuickEffects Qt6::Concurrent
)

install(TARGETS appFlaxHub
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

include(CTest)
if (BUILD_TESTING)
    add_subdirectory(tests)
endif()
