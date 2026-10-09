void Current::tick() { static LLCachedControl<bool> value(gSavedSettings, "Setting"); use(value); }
