set(_QSCI_ROOT "${CMAKE_SOURCE_DIR}/third_party/qscintilla")

# Try to find a pre-built library first
find_path(QScintilla_INCLUDE_DIR
    NAMES Qsci/qsciscintilla.h
    PATHS "${_QSCI_ROOT}/src" "${_QSCI_ROOT}/include"
    NO_DEFAULT_PATH
)

find_library(QScintilla_LIBRARY
    NAMES
        qscintilla2_qt${QT_VERSION_MAJOR} libqscintilla2_qt${QT_VERSION_MAJOR}
        qscintilla2_qt6 libqscintilla2_qt6
        qscintilla2_qt5 libqscintilla2_qt5
    PATHS
        "${_QSCI_ROOT}/src/release"
        "${_QSCI_ROOT}/src"
        "${_QSCI_ROOT}/lib"
    NO_DEFAULT_PATH
)

if(QScintilla_LIBRARY AND QScintilla_INCLUDE_DIR)
    # Use pre-built library
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(QScintilla DEFAULT_MSG
        QScintilla_LIBRARY QScintilla_INCLUDE_DIR)
    if(NOT TARGET QScintilla::QScintilla)
        add_library(QScintilla::QScintilla STATIC IMPORTED)
        set_target_properties(QScintilla::QScintilla PROPERTIES
            IMPORTED_LOCATION "${QScintilla_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${QScintilla_INCLUDE_DIR}"
        )
    endif()
elseif(EXISTS "${_QSCI_ROOT}/src/qsciscintilla.cpp")
    # Build from source
    message(STATUS "Building QScintilla from source")

    file(GLOB _QSCI_LEXER_SOURCES "${_QSCI_ROOT}/scintilla/lexers/*.cpp")
    file(GLOB _QSCI_LEXLIB_SOURCES "${_QSCI_ROOT}/scintilla/lexlib/*.cpp")
    file(GLOB _QSCI_SCI_SOURCES "${_QSCI_ROOT}/scintilla/src/*.cpp")
    file(GLOB _QSCI_HEADERS "${_QSCI_ROOT}/src/Qsci/*.h")

    set(_QSCI_QT_SOURCES
        "${_QSCI_ROOT}/src/qsciscintilla.cpp"
        "${_QSCI_ROOT}/src/qsciscintillabase.cpp"
        "${_QSCI_ROOT}/src/qsciabstractapis.cpp"
        "${_QSCI_ROOT}/src/qsciapis.cpp"
        "${_QSCI_ROOT}/src/qscicommand.cpp"
        "${_QSCI_ROOT}/src/qscicommandset.cpp"
        "${_QSCI_ROOT}/src/qscidocument.cpp"
        "${_QSCI_ROOT}/src/qscilexer.cpp"
        "${_QSCI_ROOT}/src/qscilexerasm.cpp"
        "${_QSCI_ROOT}/src/qscilexeravs.cpp"
        "${_QSCI_ROOT}/src/qscilexerbash.cpp"
        "${_QSCI_ROOT}/src/qscilexerbatch.cpp"
        "${_QSCI_ROOT}/src/qscilexercmake.cpp"
        "${_QSCI_ROOT}/src/qscilexercoffeescript.cpp"
        "${_QSCI_ROOT}/src/qscilexercpp.cpp"
        "${_QSCI_ROOT}/src/qscilexercsharp.cpp"
        "${_QSCI_ROOT}/src/qscilexercss.cpp"
        "${_QSCI_ROOT}/src/qscilexercustom.cpp"
        "${_QSCI_ROOT}/src/qscilexerd.cpp"
        "${_QSCI_ROOT}/src/qscilexerdiff.cpp"
        "${_QSCI_ROOT}/src/qscilexeredifact.cpp"
        "${_QSCI_ROOT}/src/qscilexerfortran.cpp"
        "${_QSCI_ROOT}/src/qscilexerfortran77.cpp"
        "${_QSCI_ROOT}/src/qscilexerhex.cpp"
        "${_QSCI_ROOT}/src/qscilexerhtml.cpp"
        "${_QSCI_ROOT}/src/qscilexeridl.cpp"
        "${_QSCI_ROOT}/src/qscilexerintelhex.cpp"
        "${_QSCI_ROOT}/src/qscilexerjava.cpp"
        "${_QSCI_ROOT}/src/qscilexerjavascript.cpp"
        "${_QSCI_ROOT}/src/qscilexerjson.cpp"
        "${_QSCI_ROOT}/src/qscilexerlua.cpp"
        "${_QSCI_ROOT}/src/qscilexermakefile.cpp"
        "${_QSCI_ROOT}/src/qscilexermarkdown.cpp"
        "${_QSCI_ROOT}/src/qscilexermasm.cpp"
        "${_QSCI_ROOT}/src/qscilexermatlab.cpp"
        "${_QSCI_ROOT}/src/qscilexernasm.cpp"
        "${_QSCI_ROOT}/src/qscilexeroctave.cpp"
        "${_QSCI_ROOT}/src/qscilexerpascal.cpp"
        "${_QSCI_ROOT}/src/qscilexerperl.cpp"
        "${_QSCI_ROOT}/src/qscilexerpostscript.cpp"
        "${_QSCI_ROOT}/src/qscilexerpo.cpp"
        "${_QSCI_ROOT}/src/qscilexerpov.cpp"
        "${_QSCI_ROOT}/src/qscilexerproperties.cpp"
        "${_QSCI_ROOT}/src/qscilexerpython.cpp"
        "${_QSCI_ROOT}/src/qscilexerruby.cpp"
        "${_QSCI_ROOT}/src/qscilexerspice.cpp"
        "${_QSCI_ROOT}/src/qscilexersql.cpp"
        "${_QSCI_ROOT}/src/qscilexersrec.cpp"
        "${_QSCI_ROOT}/src/qscilexertcl.cpp"
        "${_QSCI_ROOT}/src/qscilexertekhex.cpp"
        "${_QSCI_ROOT}/src/qscilexertex.cpp"
        "${_QSCI_ROOT}/src/qscilexerverilog.cpp"
        "${_QSCI_ROOT}/src/qscilexervhdl.cpp"
        "${_QSCI_ROOT}/src/qscilexerxml.cpp"
        "${_QSCI_ROOT}/src/qscilexeryaml.cpp"
        "${_QSCI_ROOT}/src/qscimacro.cpp"
        "${_QSCI_ROOT}/src/qsciprinter.cpp"
        "${_QSCI_ROOT}/src/qscistyle.cpp"
        "${_QSCI_ROOT}/src/qscistyledtext.cpp"
        "${_QSCI_ROOT}/src/InputMethod.cpp"
        "${_QSCI_ROOT}/src/ListBoxQt.cpp"
        "${_QSCI_ROOT}/src/PlatQt.cpp"
        "${_QSCI_ROOT}/src/SciAccessibility.cpp"
        "${_QSCI_ROOT}/src/SciClasses.cpp"
        "${_QSCI_ROOT}/src/ScintillaQt.cpp"
    )

    add_library(qscintilla2 STATIC
        ${_QSCI_QT_SOURCES}
        ${_QSCI_HEADERS}
        ${_QSCI_LEXER_SOURCES}
        ${_QSCI_LEXLIB_SOURCES}
        ${_QSCI_SCI_SOURCES}
    )

    target_include_directories(qscintilla2 PUBLIC
        "${_QSCI_ROOT}/src"
    )
    target_include_directories(qscintilla2 PRIVATE
        "${_QSCI_ROOT}/scintilla/include"
        "${_QSCI_ROOT}/scintilla/lexlib"
        "${_QSCI_ROOT}/scintilla/src"
    )

    target_compile_definitions(qscintilla2 PRIVATE
        SCINTILLA_QT
        SCI_LEXER
        INCLUDE_DEPRECATED_FEATURES
    )

    target_link_libraries(qscintilla2 PUBLIC
        ${QT}::Widgets
        ${QT}::PrintSupport
    )

    set_target_properties(qscintilla2 PROPERTIES AUTOMOC ON)

    add_library(QScintilla::QScintilla ALIAS qscintilla2)
    set(QScintilla_FOUND TRUE)
else()
    set(QScintilla_FOUND FALSE)
    if(QScintilla_FIND_REQUIRED)
        message(FATAL_ERROR "Could NOT find QScintilla (missing source and pre-built library)")
    endif()
endif()
