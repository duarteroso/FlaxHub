cmake_minimum_required(VERSION 3.16)

project(FlaxHub VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick QuickDialogs2 Network QuickEffects Concurrent)

qt_standard_project_setup(REQUIRES 6.10)

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
        components/ViewHeader.qml
        components/HubButton.qml
        components/SearchField.qml
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
        dialogs/QuitDialog.qml
        dialogs/UninstallDialog.qml
        dialogs/ClearFolderDialog.qml
        dialogs/NoticeDialog.qml
    RESOURCES
        icons/joystick.svg
        icons/install_desktop.svg
        icons/settings.svg
        icons/star.svg
        icons/star_filled.svg
    SOURCES
        src/installlistmodel.h
        src/installlistmodel.cpp
        src/installlayout.h
        src/installvalidator.h
        src/installvalidator.cpp
        src/projectlistmodel.h
        src/projectlistmodel.cpp
        src/pathutils.h
        src/filemanager.h
        src/filemanager.cpp
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
target_include_directories(appFlaxHub PRIVATE src)

set_target_properties(appFlaxHub PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appFlaxHub
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
