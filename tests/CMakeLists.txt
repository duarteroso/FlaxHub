find_package(Qt6 REQUIRED COMPONENTS Test Concurrent Network Qml)

# Each test is its own executable, since each one's QTEST_MAIN or QTEST_APPLESS_MAIN supplies its own main. A test
# compiles the sources it covers from src/, so it does not link the app.
add_executable(tst_installrules
    tst_installrules.cpp
    ../src/installrules.h
    ../src/installrules.cpp
    ../src/installvalidator.h
    ../src/installvalidator.cpp
    ../src/installlayout.h
    ../src/versionutils.h
)
target_include_directories(tst_installrules PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_options(tst_installrules PRIVATE ${FLAXHUB_WARNING_FLAGS})
target_link_libraries(tst_installrules PRIVATE Qt6::Test)
add_test(NAME tst_installrules COMMAND tst_installrules)

add_executable(tst_installvalidator
    tst_installvalidator.cpp
    ../src/installvalidator.h
    ../src/installvalidator.cpp
    ../src/installrules.h
    ../src/installrules.cpp
    ../src/installlayout.h
)
target_include_directories(tst_installvalidator PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_options(tst_installvalidator PRIVATE ${FLAXHUB_WARNING_FLAGS})
target_link_libraries(tst_installvalidator PRIVATE Qt6::Test)
add_test(NAME tst_installvalidator COMMAND tst_installvalidator)

# The two models with a QSettings on a temporary file. They need the whole model layer: the models use the runner,
# the catalog, the validator and the app settings' folder default. It is compiled once and linked into both tests.
add_library(flaxhub_model_objects OBJECT
    ../src/appsettings.h
    ../src/appsettings.cpp
    ../src/enginecatalog.h
    ../src/enginecatalog.cpp
    ../src/engineregistry.h
    ../src/installrunner.h
    ../src/installrunner.cpp
    ../src/installlistmodel.h
    ../src/installlistmodel.cpp
    ../src/installtable.h
    ../src/installtable.cpp
    ../src/installstore.h
    ../src/installstore.cpp
    ../src/installsteps.h
    ../src/installsteps.cpp
    ../src/installfolderjobs.h
    ../src/installfolderjobs.cpp
    ../src/installlayout.h
    ../src/installrules.h
    ../src/installrules.cpp
    ../src/installvalidator.h
    ../src/installvalidator.cpp
    ../src/projectlistmodel.h
    ../src/projectlistmodel.cpp
    ../src/projectcreator.h
    ../src/projectcreator.cpp
    ../src/pathutils.h
    ../src/settingsarray.h
    ../src/versionutils.h
)
target_include_directories(flaxhub_model_objects PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_compile_options(flaxhub_model_objects PRIVATE ${FLAXHUB_WARNING_FLAGS})
target_link_libraries(flaxhub_model_objects PUBLIC Qt6::Concurrent Qt6::Network Qt6::Qml)

add_executable(tst_installlistmodel tst_installlistmodel.cpp)
target_compile_options(tst_installlistmodel PRIVATE ${FLAXHUB_WARNING_FLAGS})
target_link_libraries(tst_installlistmodel PRIVATE flaxhub_model_objects Qt6::Test)
qt_add_translations(tst_installlistmodel TS_FILES ${PROJECT_SOURCE_DIR}/translations/flaxhub_en.ts)
add_test(NAME tst_installlistmodel COMMAND tst_installlistmodel)

add_executable(tst_projectlistmodel tst_projectlistmodel.cpp)
target_compile_options(tst_projectlistmodel PRIVATE ${FLAXHUB_WARNING_FLAGS})
target_link_libraries(tst_projectlistmodel PRIVATE flaxhub_model_objects Qt6::Test)
qt_add_translations(tst_projectlistmodel TS_FILES ${PROJECT_SOURCE_DIR}/translations/flaxhub_en.ts)
add_test(NAME tst_projectlistmodel COMMAND tst_projectlistmodel)

add_executable(tst_enginecatalog
    tst_enginecatalog.cpp
    ../src/enginecatalog.h
    ../src/enginecatalog.cpp
    ../src/versionutils.h
)
target_include_directories(tst_enginecatalog PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_options(tst_enginecatalog PRIVATE ${FLAXHUB_WARNING_FLAGS})
target_link_libraries(tst_enginecatalog PRIVATE Qt6::Test Qt6::Network)
add_test(NAME tst_enginecatalog COMMAND tst_enginecatalog)

add_executable(tst_filemanager
    tst_filemanager.cpp
    ../src/filemanager.h
    ../src/filemanager.cpp
    ../src/pathutils.h
)
target_include_directories(tst_filemanager PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_options(tst_filemanager PRIVATE ${FLAXHUB_WARNING_FLAGS})
target_link_libraries(tst_filemanager PRIVATE Qt6::Test Qt6::Gui Qt6::Qml)
add_test(NAME tst_filemanager COMMAND tst_filemanager)
