#include "includes.h"

void IEngineTrace::TraceLine(const vec3_t& src, const vec3_t& dst, int mask, IHandleEntity* entity, int collision_group, CGameTrace* trace) {
    static auto trace_filter_simple = pattern::find(g_csgo.m_client_dll, XOR("55 8B EC 83 E4 F0 83 EC 7C 56 52")) + 0x3D;

    std::uintptr_t filter[4] = { *reinterpret_cast<std::uintptr_t*>(trace_filter_simple), reinterpret_cast<std::uintptr_t>(entity), collision_group, 0 };

    TraceRay(Ray(src, dst), mask, reinterpret_cast<CTraceFilter*>(&filter), trace);
}

void IEngineTrace::TraceHull(const vec3_t& src, const vec3_t& dst, const vec3_t& mins, const vec3_t& maxs, int mask, IHandleEntity* entity, int collision_group, CGameTrace* trace) {
    static auto trace_filter_simple = pattern::find(g_csgo.m_client_dll, XOR("55 8B EC 83 E4 F0 83 EC 7C 56 52")) + 0x3D;

    std::uintptr_t filter[4] = { *reinterpret_cast<std::uintptr_t*>(trace_filter_simple), reinterpret_cast<std::uintptr_t>(entity), collision_group, 0 };

    TraceRay(Ray(src, dst, mins, maxs), mask, reinterpret_cast<CTraceFilter*>(&filter), trace);
}

void rotate_point(vec2_t& point, vec2_t origin, bool clockwise, float angle) {
    vec2_t delta = point - origin;
    vec2_t rotated;

    if (clockwise) {
        rotated = vec2_t(delta.x * cosf(angle) - delta.y * sinf(angle), delta.x * sinf(angle) + delta.y * cosf(angle));
    }
    else {
        rotated = vec2_t(delta.x * sinf(angle) - delta.y * cosf(angle), delta.x * cosf(angle) + delta.y * sinf(angle));
    }

    point = rotated + origin;
}

float& Entity::get_creation_time() {
    return *reinterpret_cast<float*>(0x29B0);
}

void c_grenade_prediction::on_create_move(CUserCmd* cmd) {
    m_data = {};

    if (!g_cl.m_processing || !g_menu.main.visuals.tracers.get())
        return;

    const auto weapon = reinterpret_cast<Weapon*>(g_csgo.m_entlist->GetClientEntityFromHandle(g_cl.m_local->GetActiveWeapon()));
    if (!weapon || !weapon->m_bPinPulled() && weapon->m_fThrowTime() == 0.f)
        return;

    const auto weapon_data = weapon->GetWpnData();
    if (!weapon_data || weapon_data->m_weapon_type != 9)
        return;

    m_data.m_owner = g_cl.m_local;
    m_data.m_index = weapon->m_iItemDefinitionIndex();

    auto view_angles = cmd->m_view_angles;

    if (view_angles.x < -90.f) {
        view_angles.x += 360.f;
    }
    else if (view_angles.x > 90.f) {
        view_angles.x -= 360.f;
    }

    view_angles.x -= (90.f - std::fabsf(view_angles.x)) * 10.f / 90.f;

    auto direction = vec3_t();

    math::AngleVectors(view_angles, direction);

    const auto throw_strength = std::clamp< float >(weapon->m_flThrowStrength(), 0.f, 1.f);
    const auto eye_pos = g_cl.m_shoot_pos;
    const auto src = vec3_t(eye_pos.x, eye_pos.y, eye_pos.z + (throw_strength * 12.f - 12.f));

    auto trace = CGameTrace();

    g_csgo.m_engine_trace->TraceHull(src, src + direction * 22.f, { -2.f, -2.f, -2.f }, { 2.f, 2.f, 2.f }, MASK_SOLID | CONTENTS_CURRENT_90, g_cl.m_local, COLLISION_GROUP_NONE, &trace);

    m_data.predict(trace.m_endpos - direction * 6.f, direction * (std::clamp< float >(weapon_data->m_throw_velocity * 0.9f, 15.f, 750.f) * (throw_strength * 0.7f + 0.3f)) + g_cl.m_local->m_vecVelocity() * 1.25f, g_csgo.m_globals->m_curtime, 0);
}

void DrawBeamPaw(vec3_t src, vec3_t end, Color color)
{
    BeamInfo_t beamInfo;
    beamInfo.m_nType = 0;
    beamInfo.m_nModelIndex = -1;
    beamInfo.m_flHaloScale = 0.f;
    beamInfo.m_flLife = 0.02f;
    beamInfo.m_flFadeLength = 10.f;
    beamInfo.m_flWidth = 2.f;
    beamInfo.m_flEndWidth = 2.f;
    beamInfo.m_pszModelName = "sprites/purplelaser1.vmt";
    beamInfo.m_flAmplitude = 0.f;
    beamInfo.m_flSpeed = 0.01f;
    beamInfo.m_nStartFrame = 0;
    beamInfo.m_flFrameRate = 0.f;
    beamInfo.m_flRed = color.r();
    beamInfo.m_flGreen = color.g();
    beamInfo.m_flBlue = color.b();
    beamInfo.m_flBrightness = color.a();
    beamInfo.m_nSegments = 2;
    beamInfo.m_bRenderable = true;
    beamInfo.m_nFlags = 0;
    beamInfo.m_vecStart = src;
    beamInfo.m_vecEnd = end;


    Beam_t* myBeam = g_csgo.m_beams->CreateBeamPoints(beamInfo);
    if (myBeam)
        g_csgo.m_beams->DrawBeam(myBeam);
}

void draw_arc(int x, int y, int radius, int start_angle, int percent, int thickness, Color color)
{
    auto precision = (2 * 3.14159265358979323846) / 30;
    auto step = 3.14159265358979323846 / 180;
    auto inner = radius - thickness;
    auto end_angle = (start_angle + percent) * step;
    auto start_angles = (start_angle * 3.14159265358979323846) / 180;

    for (; radius > inner; --radius) {
        for (auto angle = start_angles; angle < end_angle; angle += precision) {
            auto cx = std::round(x + radius * std::cos(angle));
            auto cy = std::round(y + radius * std::sin(angle));

            auto cx2 = std::round(x + radius * std::cos(angle + precision));
            auto cy2 = std::round(y + radius * std::sin(angle + precision));

            render::line(cx, cy, cx2, cy2, color);
        }
    }
}

const char* index_to_grenade_name_icon(int index)
{


    switch (index)
    {
    case SMOKE: return "k"; break;
    case HEGRENADE: return "j"; break;
    case MOLOTOV:return "l"; break;
    case 48:return "n"; break;
    }


    return "";
}

bool c_grenade_prediction::data_t::draw() const
{
	if (!g_menu.main.visuals.grenade_path.get())
		return false;

	if (m_owner && m_owner->enemy(g_cl.m_local) && !m_owner->m_bIsLocalPlayer() && !g_cl.m_local->enemy(m_owner))
		return false;

	if (m_path.size() <= 1u || g_csgo.m_globals->m_curtime >= m_expire_time)
		return false;

	int dist = g_cl.m_local->m_vecOrigin().dist_to(m_origin) / 12;

	std::string distance;
	distance = tfm::format(XOR("%i ft"), dist);

	Color col = g_menu.main.visuals.proj_color.get(); // ( dont touch this leave it as it is ) 
	col.a() = 0xb4;

	if (dist > 10) {
		col.a() *= std::clamp((10 - (dist - 15)) / 13.f, 0.f, 255.f);
	}

	auto prev_screen = vec2_t();
	auto prev_on_screen = render::WorldToScreen(std::get< vec3_t >(m_path.front()), prev_screen);
	Color negro = g_menu.main.visuals.grenade_path_col.get();


	float percent = ((m_expire_time - g_csgo.m_globals->m_curtime) / game::TICKS_TO_TIME(m_tick));
	int alpha_damage = 0;

	if (m_index == HEGRENADE && dist <= 20) {
		alpha_damage = 50 - 255 * (dist / 20);
	}

	if ((m_index == MOLOTOV || m_index == FIREBOMB) && dist <= 15) {
		alpha_damage = 50 - 255 * (dist / 15);
	}

	Color outline_warning = g_menu.main.visuals.grenade_path_col.get();

	if (dist < 30)
		outline_warning = Color(194, 58, 58);

	Color fading_out = Color(31, 31, 31);
	

	//if (percent > 1)

	//// Example usage
	//float myPercent = 0.5f; // Change this value as needed
	//Color myColor(31, 31, 31, 180); // Change this value as needed

	//Color color1 = (31, 31, 31, 180);
	//DecreaseAlpha(color1, percent);


	for (auto i = 1u; i < m_path.size(); ++i) {
		auto cur_screen = vec2_t();
		const auto cur_on_screen = render::WorldToScreen(std::get< vec3_t >(m_path.at(i)), cur_screen);

		if (prev_on_screen && cur_on_screen) {

			if (g_menu.main.visuals.grenade_path.get()) {
				//DrawBeamPaw(std::get< vec3_t >(m_path.at(i - 1)), std::get< vec3_t >(m_path.at(i)), negro); // beamcolor

				render::line(prev_screen.x, prev_screen.y, cur_screen.x, cur_screen.y, negro);
				render::line(prev_screen.x + 1, prev_screen.y, cur_screen.x, cur_screen.y, negro);
				//render::line(prev_screen.x, prev_screen.y, cur_screen.x - 1, cur_screen.y - 1, { g_menu.main.misc.grenade_tracer_warning_color.get() });
				//render::line(prev_screen.x + 1, prev_screen.y + 1, cur_screen.x, cur_screen.y, { g_menu.main.misc.grenade_tracer_warning_color.get() });
				//render::line(prev_screen.x - 1, prev_screen.y - 1, cur_screen.x, cur_screen.y, { g_menu.main.misc.grenade_tracer_warning_color.get() });
			}
		}

		prev_screen = cur_screen;
		prev_on_screen = cur_on_screen;
	}


	if (dist < 100) {
		//draw_arc(prev_screen.x, prev_screen.y - 10, 21, 0, 360 * percent, 1, Color(255, 255, 255, 200));
		render::circle(prev_screen.x, prev_screen.y - 10, 22, 360, Color(31, 31, 31, negro)); // set alpha of the color
		render::warning.string(prev_screen.x - 8, prev_screen.y - 27, Color(255, 255, 255, negro), index_to_grenade_name_icon(m_index));
		render::warning2.string(prev_screen.x - 12, prev_screen.y - 8, Color(167, 167, 171, negro), distance);//255,255,255
		render::circle_outline(prev_screen.x, prev_screen.y - 10, 21, 180, Color(30, 30, 30, negro));
		render::circle_outline(prev_screen.x, prev_screen.y - 10, 22, 180, Color(outline_warning.r(), outline_warning.g(), outline_warning.b(), 150 * percent));
		render::circle_outline(prev_screen.x, prev_screen.y - 10, 23, 180, Color(outline_warning.r(), outline_warning.g(), outline_warning.b(), 100 * percent));
		render::circle_outline(prev_screen.x, prev_screen.y - 10, 24, 180, Color(outline_warning.r(), outline_warning.g(), outline_warning.b(), 50 * percent));
	}



	auto is_on_screen = [](vec3_t origin, vec2_t& screen) -> bool
		{
			if (!render::WorldToScreen(origin, screen))
				return false;

			return (screen.x > 0 && screen.x < g_cl.m_width) && (g_cl.m_height > screen.y && screen.y > 0);
		};

	vec2_t screenPos;
	vec3_t vEnemyOrigin = m_origin;
	vec3_t vLocalOrigin = g_cl.m_local->GetAbsOrigin();
	if (!g_cl.m_local->alive())
		vLocalOrigin = g_csgo.m_input->m_camera_offset;

	if (!is_on_screen(vEnemyOrigin, screenPos))
	{
		const float wm = g_cl.m_width / 2, hm = g_cl.m_height / 2;
		vec3_t last_pos = std::get< vec3_t >(m_path.at(m_path.size() - 1));

		ang_t dir;

		g_csgo.m_engine->GetViewAngles(dir);

		float view_angle = dir.y;

		if (view_angle < 0)
			view_angle += 360;

		view_angle = DEG2RAD(view_angle);

		auto entity_angle = math::CalcAngle(vLocalOrigin, vEnemyOrigin);
		entity_angle.normalize();

		if (entity_angle.y < 0.f)
			entity_angle.y += 360.f;

		entity_angle.y = DEG2RAD(entity_angle.y);
		entity_angle.y -= view_angle;

		auto position = vec2_t(wm, hm);
		position.x -= std::clamp(vLocalOrigin.dist_to(vEnemyOrigin), 400.f, hm - 40);

		static auto alpha = 1.0f;
		

		rotate_point(position, vec2_t(wm, hm), false, entity_angle.y);

		if (dist < 45) {
			//draw_arc(position.x, position.y - 10, 19, 0, 360 * percent, 1.5, Color(255, 255, 255, 225));
			render::warning.string(position.x - 4, position.y - 20, (242, 65, 65, 220), index_to_grenade_name_icon(m_index)); //255,255,255
		}

	}
	return true;
}