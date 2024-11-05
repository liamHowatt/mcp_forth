lvgl.json: lv_conf.h
	python3 ../lvgl/scripts/gen_json/gen_json.py --output-path . --lvgl-config lv_conf.h

runtime_lvgl.c: lvgl.json lvgl_json.jq
	jq -rf lvgl_json.jq lvgl.json > runtime_lvgl.c

clean:
	rm -f lvgl.json runtime_lvgl.c
