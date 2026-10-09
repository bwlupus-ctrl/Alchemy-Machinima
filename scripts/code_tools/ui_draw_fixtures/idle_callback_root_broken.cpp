void IdleRoot::install() { gIdleCallbacks.addFunction(onTick, this); }
void IdleRoot::onTick(void*) { getChild<LLView>("name"); }
