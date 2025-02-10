project(${PROJECT_NAME})

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)


FIND_PACKAGE(Qt5 REQUIRED COMPONENTS Core Gui Widgets)
FIND_PACKAGE(Boost REQUIRED COMPONENTS program_options thread filesystem)

QT5_ADD_RESOURCES(ICONS ./Viewer.qrc)
file(GLOB_RECURSE SRCS ./*.cpp)
file(GLOB_RECURSE HEADERS ./*.h)

add_executable(${PROJECT_NAME} ${HEADERS} ${SRCS} ${ICONS})

target_include_directories(${PROJECT_NAME} PRIVATE .)

TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${Boost_LIBRARIES} Qt5::Core Qt5::Gui Qt5::Widgets FDE::common)
