# LVGL with mcp_forth

`gcc-multilib` and `libsdl2-dev:i386` needs to be installed (unless OS is 32 bit) to compile as 32 bit for Forth.

```
mkdir build
cd build
cmake -GNinja ..
ninja lvgl_workspace
./lvgl_workspace
```

It reads the files in `../forth_programs/`. Click one to run it.
At the time of writing, `lv_example_get_started_1.fs` is the only
graphical program that works. All the programs in `simple/` work.
They are simple terminal output programs. Watch your terminal
for the output.

There's a bug in `lv_file_explorer` where it will crash if you
navigate into `simple/`, navigate back out, and then click
`lv_example_get_started_1.fs`.

![menu](./readme_assets/menu.png)
![menu](./readme_assets/hello.png)

```forth
\ Basic example to create a "Hello world" label

\ Change the active screen's background color
lv_screen_active 0x003a57 lv_color_hex LV_PART_MAIN lv_obj_set_style_bg_color

\ Create a white label, set its text and align it to the center
lv_screen_active lv_label_create
dup s" Hello world!" null-term lv_label_set_text
lv_screen_active 0xffffff lv_color_hex LV_PART_MAIN lv_obj_set_style_text_color
dup LV_ALIGN_CENTER 0 0 lv_obj_align

drop     \ drop the label pointer from the stack
```
