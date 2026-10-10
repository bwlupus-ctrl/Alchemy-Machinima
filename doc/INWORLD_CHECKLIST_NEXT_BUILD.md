# In-world checklist: next build

Build: `Release\AlchemyTest.exe` (build time: ________)
Branch: `feature/director-kit-p0`

Tick each box as you go. If something looks wrong, use the report list at the end of Part 1.

## What is new in this build

- **Custom water level.** You can raise or lower the water to a skybox height. It lasts for this login only.
- **Inventory.** The inventory speed test was fixed. The folder culling setting (UIFolderViewCull) was measured and stays on.
- **Director kit (hidden).** The new Director code is in the build, but it is switched off. The Director should look and work exactly as before.

---

## Part 1. Custom water level

### Where to find it

- **Water Adjustments window:** in the menu bar, open **Water Adjustments...** (it sits next to Environment Settings... and Personal Lighting...).
- **Water settings panel:** open **Environment Settings...**, then the **Water** tab.

Both show the same controls:

- **On** box: turns the custom level on or off.
- **Slider:** drag to set the water height in metres.
- **Number box:** type a height and press Enter.
- **Set to camera height:** sets the water to your camera's height and turns On on.
- **Reset:** turns On off. The water goes back to its normal level.

### Steps

- [ ] **1. Normal water.** Leave On off. Water in your region looks the same as the last build.
- [ ] **2. Raise to a skybox height.** Turn On on. Drag the slider up to the height of the skybox ground or horizon. If you do not know that height, ask your creator. Expect: the water rises as one flat sheet, with no flashing while you drag.
- [ ] **3. Set to camera height from inside the skybox.** Fly your camera up into the skybox. Click **Set to camera height**. Expect: the number box shows about your camera's height, On is on, and the water sits at camera level.
- [ ] **4. Lower below the ground.** Drag the slider to 0. Expect: the water drops under the land. The land looks normal, with no holes.
- [ ] **5. Reset.** Click **Reset**. Expect: On is off and the water is back to normal.
- [ ] **6. Relog.** Set a custom level with On on. Log out, then log back in. Expect: On is off and the water is at normal level. The custom level is not saved between logins.
- [ ] **7. Cross into a neighbour region.** Set a clearly visible custom level. Walk or fly across a region border. Expect: the water is at the same height on both sides, with no step.
- [ ] **8. Teleport.** Teleport to a different region while On is on. Expect: the new region's water appears at the custom height straight away, with no jump.
- [ ] **9. Reflections and the horizon.** Look at the water's reflections and at the horizon. Expect: the reflections look right, and there is no hard line or gap where the water meets the far scenery. Report any seam you see.
- [ ] **10. Camera below a high water plane.** Put the camera under a raised water surface. Expect: the view goes dark, like underwater fog. **This is expected. It is not a bug.**
- [ ] **11. Type 5000, then touch the slider.** Type **5000** in the number box and press Enter. Then click or drag the slider. Expect: the 5000 is not kept. The value jumps to where the slider lands. This is a known, accepted behaviour. Retype 5000 if you need it.
- [ ] **12. Largest slider value.** Drag the slider all the way to the right. Write down the largest number it reaches.

### If something looks wrong, send Claude:

- [ ] A screenshot, with the water controls visible if you can
- [ ] The number shown in the number box
- [ ] The region name (shown at the top of the screen, or on the world map)
- [ ] The step number and what you were doing
- [ ] Whether On was on, and whether you had relogged

---

## Part 2. Inventory

- [ ] Open a big inventory, with hundreds of items or a large folder.
- [ ] Scroll from the top to the bottom and back, with the mouse wheel and with the scroll bar.
- [ ] Type a word in the search box, then clear it.
- [ ] Open and close a few folders.
- [ ] Decide how it felt: **smooth**, **the same as before**, or **worse**. Write your answer down.

### Only if it felt worse

- [ ] Open **Advanced**, then **Show Debug Settings**. Search for **UIFolderViewCull**.
- [ ] Set it to **FALSE**.
- [ ] Repeat the scroll and search test.
- [ ] Report both results (with the setting on, and with it off).
- [ ] Set **UIFolderViewCull** back to **TRUE** when you are done. It is the normal setting.

### Optional: Tracy capture

- [ ] Ask Claude to run a Tracy capture while you scroll the inventory. Claude does the capture. You only do the scrolling.
- [ ] Claude compares the result with the old **5.3 ms** baseline for the inventory.

---

## Part 3. Nothing should look different

The new Director kit code is in this build, but it is switched off. Check these five spots. They should look and behave as they did in the last build.

- [ ] **1. Director pages scroll.** Open the Director. It should open on the tab you were last on. Scroll a long page, such as Props or Camera, with the mouse wheel. Nothing should be cut off.
- [ ] **2. Compass dial.** Open **Actor Mover**. Drag the heading compass dial. Then click it and use the arrow keys. It should respond the same way as before.
- [ ] **3. Light Rig cards.** Open **Cine Light Rig**. The cards should line up with no overlap, and the sliders should move.
- [ ] **4. Lightbox tabs.** Open the Lightbox and click through each tab. Tall tabs should scroll.
- [ ] **5. No new Director.** Check the menus and the Director window. You should see no new "Director v2" item and no second Director window.

If any of these look different from the last build, send Claude a screenshot and the spot number.
