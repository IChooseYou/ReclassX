#include "theme.h"

namespace rcx {

// ── Field table for DRY serialization ──

struct ColorField { const char* key; QColor Theme::*ptr; };

static const ColorField kFields[] = {
    {"background",    &Theme::background},
    {"backgroundAlt", &Theme::backgroundAlt},
    {"surface",       &Theme::surface},
    {"border",        &Theme::border},
    {"borderFocused", &Theme::borderFocused},
    {"button",        &Theme::button},
    {"text",          &Theme::text},
    {"textDim",       &Theme::textDim},
    {"textMuted",     &Theme::textMuted},
    {"textFaint",     &Theme::textFaint},
    {"hover",         &Theme::hover},
    {"selected",      &Theme::selected},
    {"selection",     &Theme::selection},
    {"syntaxKeyword", &Theme::syntaxKeyword},
    {"syntaxNumber",  &Theme::syntaxNumber},
    {"syntaxString",  &Theme::syntaxString},
    {"syntaxComment", &Theme::syntaxComment},
    {"syntaxPreproc", &Theme::syntaxPreproc},
    {"syntaxType",    &Theme::syntaxType},
    {"indHoverSpan",  &Theme::indHoverSpan},
    {"indCmdPill",    &Theme::indCmdPill},
    {"indDataChanged",&Theme::indDataChanged},
    {"indHintGreen",  &Theme::indHintGreen},
    {"markerPtr",     &Theme::markerPtr},
    {"markerCycle",   &Theme::markerCycle},
    {"markerError",   &Theme::markerError},
};

QJsonObject Theme::toJson() const {
    QJsonObject o;
    o["name"] = name;
    for (const auto& f : kFields)
        o[f.key] = (this->*f.ptr).name();
    return o;
}

Theme Theme::fromJson(const QJsonObject& o) {
    Theme t = reclassDark();
    t.name = o["name"].toString(t.name);
    for (const auto& f : kFields) {
        if (o.contains(f.key))
            t.*f.ptr = QColor(o[f.key].toString());
    }
    return t;
}

// ── Built-in themes ──

Theme Theme::reclassDark() {
    Theme t;
    t.name          = "Reclass Dark";
    t.background    = QColor("#1e1e1e");
    t.backgroundAlt = QColor("#252526");
    t.surface       = QColor("#2a2d2e");
    t.border        = QColor("#3c3c3c");
    t.borderFocused = QColor("#64e6b450");  // indHoverSpan at ~40% alpha
    t.button        = QColor("#333333");
    t.text          = QColor("#d4d4d4");
    t.textDim       = QColor("#858585");
    t.textMuted     = QColor("#585858");
    t.textFaint     = QColor("#505050");
    t.hover         = QColor("#2b2b2b");
    t.selected      = QColor("#232323");
    t.selection     = QColor("#2b2b2b");
    t.syntaxKeyword = QColor("#569cd6");
    t.syntaxNumber  = QColor("#b5cea8");
    t.syntaxString  = QColor("#ce9178");
    t.syntaxComment = QColor("#6a9955");
    t.syntaxPreproc = QColor("#c586c0");
    t.syntaxType    = QColor("#4EC9B0");
    t.indHoverSpan  = QColor("#E6B450");
    t.indCmdPill    = QColor("#2a2a2a");
    t.indDataChanged= QColor("#8fbc7a");
    t.indHintGreen  = QColor("#5a8248");
    t.markerPtr     = QColor("#f44747");
    t.markerCycle   = QColor("#e5a00d");
    t.markerError   = QColor("#7a2e2e");
    return t;
}

Theme Theme::warm() {
    Theme t;
    t.name          = "Warm";
    t.background    = QColor("#212121");
    t.backgroundAlt = QColor("#2a2a2a");
    t.surface       = QColor("#2a2a2a");
    t.border        = QColor("#373737");
    t.borderFocused = QColor("#64aa9565");  // indHoverSpan at ~40% alpha
    t.button        = QColor("#373737");
    t.text          = QColor("#AAA99F");
    t.textDim       = QColor("#7a7a6e");
    t.textMuted     = QColor("#555550");
    t.textFaint     = QColor("#464646");
    t.hover         = QColor("#373737");
    t.selected      = QColor("#2d2d2d");
    t.selection     = QColor("#21213A");
    t.syntaxKeyword = QColor("#AA9565");
    t.syntaxNumber  = QColor("#AAA98C");
    t.syntaxString  = QColor("#6B3B21");
    t.syntaxComment = QColor("#464646");
    t.syntaxPreproc = QColor("#AA9565");
    t.syntaxType    = QColor("#6B959F");
    t.indHoverSpan  = QColor("#AA9565");
    t.indCmdPill    = QColor("#2a2a2a");
    t.indDataChanged= QColor("#6B959F");
    t.indHintGreen  = QColor("#464646");
    t.markerPtr     = QColor("#6B3B21");
    t.markerCycle   = QColor("#AA9565");
    t.markerError   = QColor("#3C2121");
    return t;
}

} // namespace rcx
