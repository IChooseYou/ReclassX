set(_QSCI_ROOT "${CMAKE_SOURCE_DIR}/third_party/qscintilla")

find_path(QScintilla_INCLUDE_DIR
    NAMES Qsci/qsciscintilla.h
    PATHS "${_QSCI_ROOT}/src" "${_QSCI_ROOT}/include"
    NO_DEFAULT_PATH
)

find_library(QScintilla_LIBRARY
    NAMES qscintilla2_qt6 libqscintilla2_qt6
    PATHS
        "${_QSCI_ROOT}/src/release"
        "${_QSCI_ROOT}/src"
        "${_QSCI_ROOT}/lib"
    NO_DEFAULT_PATH
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(QScintilla DEFAULT_MSG
    QScintilla_LIBRARY QScintilla_INCLUDE_DIR)

if(QScintilla_FOUND)
    set(QScintilla_INCLUDE_DIRS ${QScintilla_INCLUDE_DIR})
    set(QScintilla_LIBRARIES ${QScintilla_LIBRARY})
    if(NOT TARGET QScintilla::QScintilla)
        add_library(QScintilla::QScintilla STATIC IMPORTED)
        set_target_properties(QScintilla::QScintilla PROPERTIES
            IMPORTED_LOCATION "${QScintilla_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${QScintilla_INCLUDE_DIR}"
        )
    endif()
endif()
