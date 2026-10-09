void onTick(void*) { findChildView("name"); }
void install() { gIdleCallbacks.addFunction(onTick, nullptr); }
