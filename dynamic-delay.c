#include <obs-module.h>
#include <util/circlebuf.h>
#include "dynamic-delay.h"
#include "easing.h"

struct frame {
	gs_texrender_t *render;
	uint64_t ts;
};

struct dynamic_delay_info {
	obs_source_t *source;
	struct circlebuf frames;
	obs_hotkey_id skip_begin_hotkey;
	obs_hotkey_id skip_end_hotkey;
	obs_hotkey_id forward_hotkey;
	obs_hotkey_id forward_slow_hotkey;
	obs_hotkey_id forward_fast_hotkey;
	obs_hotkey_id backward_hotkey;
	obs_hotkey_id backward_slow_hotkey;
	obs_hotkey_id backward_fast_hotkey;
	obs_hotkey_id pause_hotkey;
	double max_duration;
	double speed;
	double start_speed;
	double target_speed;

	double slow_forward_speed;
	double fast_forward_speed;
	double slow_backward_speed;
	double fast_backward_speed;

	uint32_t cx;
	uint32_t cy;
	bool processed_frame;
	double time_diff;
	bool target_valid;

	uint32_t easing;
	float easing_duration;
	float easing_max_duration;
	uint64_t easing_started;
};

static const char *dynamic_delay_get_name(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return obs_module_text("DynamicDelay");
}

static void free_textures(struct dynamic_delay_info *f)
{
	obs_enter_graphics();
	while (f->frames.size) {
		struct frame frame;
		circlebuf_pop_front(&f->frames, &frame, sizeof(frame));
		gs_texrender_destroy(frame.render);
	}
	circlebuf_free(&f->frames);
	obs_leave_graphics();
}

static void *dynamic_delay_create(obs_data_t *settings, obs_source_t *source)
{
	struct dynamic_delay_info *d =
		bzalloc(sizeof(struct dynamic_delay_info));
	d->source = source;
	d->speed = 1.0;
	d->target_speed = 1.0;
	d->start_speed = 1.0;
	return d;
}

static void dynamic_delay_destroy(void *data)
{
	struct dynamic_delay_info *c = data;
	obs_hotkey_unregister(c->skip_begin_hotkey);
	obs_hotkey_unregister(c->skip_end_hotkey);
	obs_hotkey_unregister(c->forward_hotkey);
	obs_hotkey_unregister(c->forward_slow_hotkey);
	obs_hotkey_unregister(c->forward_fast_hotkey);
	obs_hotkey_unregister(c->backward_hotkey);
	obs_hotkey_unregister(c->backward_slow_hotkey);
	obs_hotkey_unregister(c->backward_fast_hotkey);
	obs_hotkey_unregister(c->pause_hotkey);
	free_textures(c);
	bfree(c);
}

static void dynamic_delay_update(void *data, obs_data_t *settings)
{
	struct dynamic_delay_info *d = data;
	d->max_duration = obs_data_get_double(settings, S_DURATION);
	d->easing = obs_data_get_int(settings, S_EASING);
	d->easing_max_duration =
		(float)obs_data_get_double(settings, S_EASING_DURATION);
	d->slow_forward_speed =
		obs_data_get_double(settings, S_SLOW_FORWARD) / 100.0;
	d->fast_forward_speed =
		obs_data_get_double(settings, S_FAST_FORWARD) / 100.0;
	d->slow_backward_speed =
		obs_data_get_double(settings, S_SLOW_BACKWARD) / 100.0;
	d->fast_backward_speed =
		obs_data_get_double(settings, S_FAST_BACKWARD) / 100.0;
}

void dynamic_delay_skip_begin_hotkey(void *data, obs_hotkey_id id,
				       obs_hotkey_t *hotkey, bool pressed)
{
	if (!pressed)
		return;
	struct dynamic_delay_info *d = data;
	if (d->start_speed < 1.0 || d->speed < 1.0) {
		d->start_speed = 1.0;
		d->target_speed = 1.0;
		d->easing_started = 0;
	}
	d->time_diff = d->max_duration;
}

void dynamic_delay_skip_end_hotkey(void *data, obs_hotkey_id id,
				     obs_hotkey_t *hotkey, bool pressed)
{
	if (!pressed)
		return;
	struct dynamic_delay_info *d = data;
	if (d->start_speed > 1.0 || d->speed > 1.0) {
		d->start_speed = 1.0;
		d->target_speed = 1.0;
		d->easing_started = 0;
	}
	d->time_diff = 0;
}

void dynamic_delay_slow_forward_hotkey(void *data, obs_hotkey_id id,
				       obs_hotkey_t *hotkey, bool pressed)
{
	if (!pressed)
		return;
	struct dynamic_delay_info *d = data;
	d->start_speed = d->speed;
	d->target_speed = d->slow_forward_speed;
	d->easing_started = 0;
}

void dynamic_delay_forward_hotkey(void *data, obs_hotkey_id id,
				  obs_hotkey_t *hotkey, bool pressed)
{
	if (!pressed)
		return;
	struct dynamic_delay_info *d = data;
	d->start_speed = d->speed;
	d->target_speed = 1.0;
	d->easing_started = 0;
}

void dynamic_delay_fast_forward_hotkey(void *data, obs_hotkey_id id,
				       obs_hotkey_t *hotkey, bool pressed)
{
	if (!pressed)
		return;
	struct dynamic_delay_info *d = data;
	d->start_speed = d->speed;
	d->target_speed = d->fast_forward_speed;
	d->easing_started = 0;
}

void dynamic_delay_slow_backward_hotkey(void *data, obs_hotkey_id id,
				       obs_hotkey_t *hotkey, bool pressed)
{
	if (!pressed)
		return;
	struct dynamic_delay_info *d = data;
	d->start_speed = d->speed;
	d->target_speed = d->slow_backward_speed;
	d->easing_started = 0;
}

void dynamic_delay_backward_hotkey(void *data, obs_hotkey_id id,
				  obs_hotkey_t *hotkey, bool pressed)
{
	if (!pressed)
		return;
	struct dynamic_delay_info *d = data;
	d->start_speed = d->speed;
	d->target_speed = 1.0;
	d->easing_started = 0;
}

void dynamic_delay_fast_backward_hotkey(void *data, obs_hotkey_id id,
				       obs_hotkey_t *hotkey, bool pressed)
{
	if (!pressed)
		return;
	struct dynamic_delay_info *d = data;
	d->start_speed = d->speed;
	d->target_speed = d->fast_backward_speed;
	d->easing_started = 0;
}

void dynamic_delay_pause_hotkey(void *data, obs_hotkey_id id,
					obs_hotkey_t *hotkey, bool pressed)
{
	if (!pressed)
		return;
	struct dynamic_delay_info *d = data;
	d->start_speed = d->speed;
	d->target_speed = 0.0;
	d->easing_started = 0;
}

static void dynamic_delay_load(void *data, obs_data_t *settings)
{
	struct dynamic_delay_info *d = data;
	obs_source_t *parent = obs_filter_get_parent(d->source);
	if (parent) {
		d->skip_begin_hotkey = obs_hotkey_register_source(
			parent, "skip_begin", obs_module_text("SkipBegin"),
			dynamic_delay_skip_begin_hotkey, data);
		d->skip_end_hotkey = obs_hotkey_register_source(
			parent, "skip_begin", obs_module_text("SkipEnd"),
			dynamic_delay_skip_end_hotkey, data);
		d->forward_hotkey = obs_hotkey_register_source(
			parent, "forward", obs_module_text("Forward"),
			dynamic_delay_forward_hotkey, data);
		d->forward_slow_hotkey = obs_hotkey_register_source(
			parent, "slow_forward", obs_module_text("SlowForward"),
			dynamic_delay_slow_forward_hotkey, data);
		d->forward_fast_hotkey = obs_hotkey_register_source(
			parent, "fast_forward", obs_module_text("FastForward"),
			dynamic_delay_fast_forward_hotkey, data);
		d->backward_hotkey = obs_hotkey_register_source(
			parent, "backward", obs_module_text("Backward"),
			dynamic_delay_backward_hotkey, data);
		d->backward_slow_hotkey = obs_hotkey_register_source(
			parent, "slow_backward", obs_module_text("SlowBackward"),
			dynamic_delay_slow_backward_hotkey, data);
		d->backward_fast_hotkey = obs_hotkey_register_source(
			parent, "fast_backward", obs_module_text("FastBackward"),
			dynamic_delay_fast_backward_hotkey, data);
		d->pause_hotkey = obs_hotkey_register_source(
			parent, "pause",
			obs_module_text("Pause"),
			dynamic_delay_pause_hotkey, data);
	}
	dynamic_delay_update(data, settings);
}

static void draw_frame(struct dynamic_delay_info *d)
{
	struct frame *frame = NULL;
	if (!d->frames.size)
		return;
	const size_t count = d->frames.size / sizeof(struct frame);
	if (d->time_diff <= 0.0) {
		frame = circlebuf_data(&d->frames,
				       (count - 1) * sizeof(struct frame));
	} else {
		size_t i = 0;
		const uint64_t ts = obs_get_video_frame_time();
		while (i < count) {
			frame = circlebuf_data(&d->frames,
					       i * sizeof(struct frame));
			if (ts - frame->ts <
			    (uint64_t)(d->time_diff * 1000000000.0))
				break;
			i++;
		}
	}
	if (!frame)
		return;

	gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_texture_t *tex = gs_texrender_get_texture(frame->render);
	if (tex) {
		gs_eparam_t *image =
			gs_effect_get_param_by_name(effect, "image");
		gs_effect_set_texture(image, tex);

		while (gs_effect_loop(effect, "Draw"))
			gs_draw_sprite(tex, 0, d->cx, d->cy);
	}
}

static void dynamic_delay_video_render(void *data, gs_effect_t *effect)
{
	struct dynamic_delay_info *d = data;
	obs_source_t *target = obs_filter_get_target(d->source);
	obs_source_t *parent = obs_filter_get_parent(d->source);

	if (!d->target_valid || !target || !parent) {
		obs_source_skip_video_filter(d->source);
		return;
	}
	if (d->processed_frame) {
		draw_frame(d);
		return;
	}

	const uint64_t ts = obs_get_video_frame_time();
	struct frame frame;
	frame.render = NULL;
	if (d->frames.size) {
		circlebuf_peek_front(&d->frames, &frame, sizeof(frame));
		if (ts - frame.ts <
		    (uint64_t)(d->max_duration * 1000000000.0)) {
			frame.render = NULL;
		} else {
			circlebuf_pop_front(&d->frames, &frame, sizeof(frame));
		}
	}
	if (!frame.render) {
		frame.render = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	} else {
		gs_texrender_reset(frame.render);
	}
	frame.ts = ts;

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

	if (gs_texrender_begin(frame.render, d->cx, d->cy)) {
		uint32_t parent_flags = obs_source_get_output_flags(target);
		bool custom_draw = (parent_flags & OBS_SOURCE_CUSTOM_DRAW) != 0;
		bool async = (parent_flags & OBS_SOURCE_ASYNC) != 0;
		struct vec4 clear_color;

		vec4_zero(&clear_color);
		gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
		gs_ortho(0.0f, (float)d->cx, 0.0f, (float)d->cy, -100.0f,
			 100.0f);

		if (target == parent && !custom_draw && !async)
			obs_source_default_render(target);
		else
			obs_source_video_render(target);

		gs_texrender_end(frame.render);
	}

	gs_blend_state_pop();

	circlebuf_push_back(&d->frames, &frame, sizeof(frame));

	draw_frame(d);
	d->processed_frame = true;
}

static obs_properties_t *dynamic_delay_properties(void *data)
{
	obs_properties_t *ppts = obs_properties_create();
	obs_property_t *p = obs_properties_add_float(
		ppts, S_DURATION, obs_module_text("Duration"), 0.0, 1000, 1.0);
	obs_property_float_set_suffix(p, "s");

	p = obs_properties_add_list(ppts, S_EASING, obs_module_text("Easing"),
				    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

	obs_property_list_add_int(p, obs_module_text("EasingFunction.Linear"),
				  EASING_LINEAR);
	obs_property_list_add_int(p,
				  obs_module_text("EasingFunction.Quadratic"),
				  EASING_QUADRATIC);
	obs_property_list_add_int(p, obs_module_text("EasingFunction.Cubic"),
				  EASING_CUBIC);
	obs_property_list_add_int(p, obs_module_text("EasingFunction.Quartic"),
				  EASING_QUARTIC);
	obs_property_list_add_int(p, obs_module_text("EasingFunction.Quintic"),
				  EASING_QUINTIC);
	obs_property_list_add_int(p, obs_module_text("EasingFunction.Sine"),
				  EASING_SINE);
	obs_property_list_add_int(p, obs_module_text("EasingFunction.Circular"),
				  EASING_CIRCULAR);
	obs_property_list_add_int(p,
				  obs_module_text("EasingFunction.Exponential"),
				  EASING_EXPONENTIAL);
	obs_property_list_add_int(p, obs_module_text("EasingFunction.Elastic"),
				  EASING_ELASTIC);
	obs_property_list_add_int(p, obs_module_text("EasingFunction.Bounce"),
				  EASING_BOUNCE);
	obs_property_list_add_int(p, obs_module_text("EasingFunction.Back"),
				  EASING_BACK);

	p = obs_properties_add_float(ppts, S_EASING_DURATION,
				     obs_module_text("EasingDuration"), 0.0,
				     100, 1.0);
	obs_property_float_set_suffix(p, "s");

	p = obs_properties_add_float_slider(ppts, S_SLOW_FORWARD,
					    obs_module_text("SlowForward"), 0.1,
					    99.9, 1.0);
	obs_property_float_set_suffix(p, "%");

	p = obs_properties_add_float_slider(ppts, S_FAST_FORWARD,
					    obs_module_text("FastForward"),
					    100.1, 1000.0, 1.0);
	obs_property_float_set_suffix(p, "%");

	p = obs_properties_add_float_slider(ppts, S_SLOW_BACKWARD,
					    obs_module_text("SlowBackward"),
					    -99.9, -0.1, 1.0);
	obs_property_float_set_suffix(p, "%");

	p = obs_properties_add_float_slider(ppts, S_FAST_BACKWARD,
					    obs_module_text("FastBackward"),
					    -1000.0, -100.1, 1.0);
	obs_property_float_set_suffix(p, "%");

	return ppts;
}

void dynamic_delay_defaults(obs_data_t *settings)
{
	obs_data_set_default_double(settings, S_DURATION, 10.0);
	obs_data_set_default_double(settings, S_EASING_DURATION, 1.0);
	obs_data_set_default_int(settings, S_EASING, EASING_CUBIC);
	obs_data_set_default_double(settings, S_SLOW_FORWARD, 50.0);
	obs_data_set_default_double(settings, S_FAST_FORWARD, 200.0);
	obs_data_set_default_double(settings, S_SLOW_BACKWARD, -50.0);
	obs_data_set_default_double(settings, S_FAST_BACKWARD, -200.0);
}

static inline void check_size(struct dynamic_delay_info *d)
{
	obs_source_t *target = obs_filter_get_target(d->source);

	d->target_valid = !!target;
	if (!d->target_valid)
		return;

	const uint32_t cx = obs_source_get_base_width(target);
	const uint32_t cy = obs_source_get_base_height(target);

	d->target_valid = !!cx && !!cy;
	if (!d->target_valid)
		return;

	if (cx != d->cx || cy != d->cy) {
		d->cx = cx;
		d->cy = cy;
		free_textures(d);
	}
}

static void dynamic_delay_tick(void *data, float t)
{
	struct dynamic_delay_info *d = data;
	d->processed_frame = false;
	if (d->speed != d->target_speed) {
		const uint64_t ts = obs_get_video_frame_time();
		if (!d->easing_started)
			d->easing_started = ts;
		const double duration =
			(double)(ts - d->easing_started) / 1000000000.0;
		if (duration > d->easing_max_duration) {
			d->speed = d->target_speed;
		} else {
			double t2 = duration / d->easing_max_duration;
			if (d->easing == EASING_QUADRATIC) {
				t2 = QuadraticEaseInOut(t2);
			} else if (d->easing == EASING_CUBIC) {
				t2 = CubicEaseInOut(t2);
			} else if (d->easing == EASING_QUARTIC) {
				t2 = QuarticEaseInOut(t2);
			} else if (d->easing == EASING_QUINTIC) {
				t2 = QuinticEaseInOut(t2);
			} else if (d->easing == EASING_SINE) {
				t2 = SineEaseInOut(t2);
			} else if (d->easing == EASING_CIRCULAR) {
				t2 = CircularEaseInOut(t2);
			} else if (d->easing == EASING_EXPONENTIAL) {
				t2 = ExponentialEaseInOut(t2);
			} else if (d->easing == EASING_ELASTIC) {
				t2 = ElasticEaseInOut(t2);
			} else if (d->easing == EASING_BOUNCE) {
				t2 = BounceEaseInOut(t2);
			} else if (d->easing == EASING_BACK) {
				t2 = BackEaseInOut(t2);
			}
			d->speed = d->start_speed +
				   (d->target_speed - d->start_speed) * t2;
		}
	} else if (d->easing_started) {
		d->easing_started = 0;
	}
	if (d->speed > 1.0 && d->target_speed > 1.0 &&
	    d->time_diff < d->easing_max_duration / 2.0) {
		d->start_speed = d->speed;
		d->target_speed = 1.0;
		d->easing_started = 0;
	} else if (d->speed < 1.0 && d->target_speed < 1.0 &&
		   d->max_duration - d->time_diff <
			   d->easing_max_duration / 2.0) {
		d->start_speed = d->speed;
		d->target_speed = 1.0;
		d->easing_started = 0;
	}

	d->time_diff += (1.0 - d->speed) * t;
	if (d->time_diff < 0.0)
		d->time_diff = 0.0;
	if (d->time_diff > d->max_duration)
		d->time_diff = d->max_duration;

	check_size(d);
}

void dynamic_delay_activate(void *data)
{
	struct dynamic_delay_info *d = data;
}

void dynamic_delay_deactivate(void *data)
{
	struct dynamic_delay_info *d = data;
}

void dynamic_delay_show(void *data)
{
	struct dynamic_delay_info *d = data;
}

void dynamic_delay_hide(void *data)
{
	struct dynamic_delay_info *d = data;
}

struct obs_source_info dynamic_delay_filter = {
	.id = "dynamic_delay_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_OUTPUT_VIDEO,
	.get_name = dynamic_delay_get_name,
	.create = dynamic_delay_create,
	.destroy = dynamic_delay_destroy,
	.load = dynamic_delay_load,
	.update = dynamic_delay_update,
	.video_render = dynamic_delay_video_render,
	.get_properties = dynamic_delay_properties,
	.get_defaults = dynamic_delay_defaults,
	.video_tick = dynamic_delay_tick,
	.activate = dynamic_delay_activate,
	.deactivate = dynamic_delay_deactivate,
	.show = dynamic_delay_show,
	.hide = dynamic_delay_hide,
};

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Exeldro");
OBS_MODULE_USE_DEFAULT_LOCALE("dynamic-delay", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("Description");
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return obs_module_text("DynamicDelay");
}

bool obs_module_load(void)
{
	obs_register_source(&dynamic_delay_filter);
	return true;
}
