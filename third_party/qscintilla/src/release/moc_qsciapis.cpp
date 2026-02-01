/****************************************************************************
** Meta object code from reading C++ file 'qsciapis.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.5.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../Qsci/qsciapis.h"
#include <QtCore/qmetatype.h>

#if __has_include(<QtCore/qtmochelpers.h>)
#include <QtCore/qtmochelpers.h>
#else
QT_BEGIN_MOC_NAMESPACE
#endif


#include <memory>

#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qsciapis.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.5.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {

#ifdef QT_MOC_HAS_STRINGDATA
struct qt_meta_stringdata_CLASSQsciAPIsENDCLASS_t {};
static constexpr auto qt_meta_stringdata_CLASSQsciAPIsENDCLASS = QtMocHelpers::stringData(
    "QsciAPIs",
    "apiPreparationCancelled",
    "",
    "apiPreparationStarted",
    "apiPreparationFinished"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSQsciAPIsENDCLASS_t {
    uint offsetsAndSizes[10];
    char stringdata0[9];
    char stringdata1[24];
    char stringdata2[1];
    char stringdata3[22];
    char stringdata4[23];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CLASSQsciAPIsENDCLASS_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CLASSQsciAPIsENDCLASS_t qt_meta_stringdata_CLASSQsciAPIsENDCLASS = {
    {
        QT_MOC_LITERAL(0, 8),  // "QsciAPIs"
        QT_MOC_LITERAL(9, 23),  // "apiPreparationCancelled"
        QT_MOC_LITERAL(33, 0),  // ""
        QT_MOC_LITERAL(34, 21),  // "apiPreparationStarted"
        QT_MOC_LITERAL(56, 22)   // "apiPreparationFinished"
    },
    "QsciAPIs",
    "apiPreparationCancelled",
    "",
    "apiPreparationStarted",
    "apiPreparationFinished"
};
#undef QT_MOC_LITERAL
#endif // !QT_MOC_HAS_STRING_DATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSQsciAPIsENDCLASS[] = {

 // content:
      11,       // revision
       0,       // classname
       0,    0, // classinfo
       3,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       3,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   32,    2, 0x06,    1 /* Public */,
       3,    0,   33,    2, 0x06,    2 /* Public */,
       4,    0,   34,    2, 0x06,    3 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject QsciAPIs::staticMetaObject = { {
    QMetaObject::SuperData::link<QsciAbstractAPIs::staticMetaObject>(),
    qt_meta_stringdata_CLASSQsciAPIsENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSQsciAPIsENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSQsciAPIsENDCLASS_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QsciAPIs, std::true_type>,
        // method 'apiPreparationCancelled'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'apiPreparationStarted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'apiPreparationFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void QsciAPIs::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<QsciAPIs *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->apiPreparationCancelled(); break;
        case 1: _t->apiPreparationStarted(); break;
        case 2: _t->apiPreparationFinished(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (QsciAPIs::*)();
            if (_t _q_method = &QsciAPIs::apiPreparationCancelled; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (QsciAPIs::*)();
            if (_t _q_method = &QsciAPIs::apiPreparationStarted; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (QsciAPIs::*)();
            if (_t _q_method = &QsciAPIs::apiPreparationFinished; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
    }
    (void)_a;
}

const QMetaObject *QsciAPIs::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QsciAPIs::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSQsciAPIsENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QsciAbstractAPIs::qt_metacast(_clname);
}

int QsciAPIs::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QsciAbstractAPIs::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 3)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 3;
    }
    return _id;
}

// SIGNAL 0
void QsciAPIs::apiPreparationCancelled()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void QsciAPIs::apiPreparationStarted()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void QsciAPIs::apiPreparationFinished()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}
QT_WARNING_POP
