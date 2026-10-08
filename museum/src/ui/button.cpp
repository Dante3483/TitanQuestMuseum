#include "ui/button.h"
namespace museum::ui {
void Button::draw(Renderer& r,int size,float padding,bool over,bool down) const {
    const Color face = down ? ground : marked ? match : selected ? ui::selected : over ? hover : normal;
    r.fill(rect,face); r.outline(rect,selected ? gold : hover);
    wchar_t text[64]; widen(caption,text,64);
    TextRenderer tr(r);
    // Fitting is local to this button. It cannot mutate panel style or other controls.
    const int local = tr.fit(text,size,6,rect.w-2*padding);
    if (local) tr.drawCentered(rect,text,local,selected && !marked ? dark : dimmed || !enabled ? hover : gold);
}
}
