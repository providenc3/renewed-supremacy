#include "includes.h"

Resolver g_resolver{};;

#pragma optimize( "", off )

float Resolver::AntiFreestand(Player* player, LagRecord* record, vec3_t start_, vec3_t end, bool include_base, float base_yaw, float delta) {
	AimPlayer* data = &g_aimbot.m_players[player->index() - 1];

	// constants.
	constexpr float STEP{ 4.f };
	constexpr float RANGE{ 32.f };

	// construct vector of angles to test.
	std::vector< AdaptiveAngle > angles{ };

	angles.emplace_back(base_yaw + delta);
	angles.emplace_back(base_yaw - delta);

	if (include_base/* || g_menu.main.aimbot.resolver_modifiers.get(1)*/)
		angles.emplace_back(base_yaw);

	// start the trace at the enemy shoot pos.
	vec3_t start = start_;

	// see if we got any valid result.
	// if this is false the path was not obstructed with anything.
	bool valid{ false };

	// get the enemies shoot pos.
	vec3_t shoot_pos = end;

	// iterate vector of angles.
	for (auto it = angles.begin(); it != angles.end(); ++it) {

		// compute the 'rough' estimation of where our head will be.
		vec3_t end{ shoot_pos.x + std::cos(math::deg_to_rad(it->m_yaw)) * RANGE,
			shoot_pos.y + std::sin(math::deg_to_rad(it->m_yaw)) * RANGE,
			shoot_pos.z };

		// draw a line for debugging purposes.
		//g_csgo.m_debug_overlay->AddLineOverlay( start, end, 255, 0, 0, true, 0.1f );

		// compute the direction.
		vec3_t dir = end - start;
		float len = dir.normalize();

		// should never happen.
		if (len <= 0.f)
			continue;

		// step thru the total distance, 4 units per step.
		for (float i{ 0.f }; i < len; i += STEP) {
			// get the current step position.
			vec3_t point = start + (dir * i);

			// get the contents at this point.
			int contents = g_csgo.m_engine_trace->GetPointContents(point, MASK_SHOT_HULL);

			// contains nothing that can stop a bullet.
			if (!(contents & MASK_SHOT_HULL))
				continue;

			float mult = 1.f;

			// over 50% of the total length, prioritize this shit.
			if (i > (len * 0.5f))
				mult = 1.25f;

			// over 90% of the total length, prioritize this shit.
			if (i > (len * 0.75f))
				mult = 1.25f;

			// over 90% of the total length, prioritize this shit.
			if (i > (len * 0.9f))
				mult = 2.f;

			// append 'penetrated distance'.
			it->m_dist += (STEP * mult);

			// mark that we found anything.
			valid = true;
		}
	}

	if (!valid)
		return base_yaw;

	// put the most distance at the front of the container.
	std::sort(angles.begin(), angles.end(),
		[](const AdaptiveAngle& a, const AdaptiveAngle& b) {
			return a.m_dist > b.m_dist;
		});

	// the best angle should be at the front now.
	return angles.front().m_yaw;
}

LagRecord* Resolver::FindIdealRecord(AimPlayer* data) {
	LagRecord* first_valid, * current, * first_flick;

	if (data->m_records.empty())
		return nullptr;

	first_flick = nullptr;
	first_valid = nullptr;


	LagRecord* front = data->m_records.front().get();

	if (front && (front->broke_lc() || front->m_sim_time < front->m_old_sim_time)) {


		if (front->valid())
			return front;

		return nullptr;
	}

	// iterate records.
	for (const auto& it : data->m_records) {
		if (it->dormant() || it->immune() || !it->valid())
			continue;

		// get current record.
		current = it.get();

		// ok, stop lagcompensating here
		if (current->broke_lc())
			break;

		// first record that was valid, store it for later.
		if (!first_valid)
			first_valid = current;

		if (!first_flick && (it->m_mode == Modes::RESOLVE_LBY || it->m_mode == Modes::RESOLVE_LBY_PRED))
			first_flick = current;

		// try to find a record with a shot, lby update, walking or no anti-aim.
		if (it->m_mode == Modes::RESOLVE_WALK && it->m_ground_for_two_ticks) {
			// only shoot at ideal record if we can hit head (usually baim is faster if we shoot at front)
			if (it->m_origin.dist_to(data->m_records.front()->m_origin) <= 0.1f || g_aimbot.CanHitRecordHead(current))
				return current;
		}
	}

	if (first_flick)
		return first_flick;

	// none found above, return the first valid record if possible.
	return (first_valid) ? first_valid : nullptr;
}

LagRecord* Resolver::FindLastRecord(AimPlayer* data) {
	LagRecord* current;

	if (data->m_records.empty())
		return nullptr;

	LagRecord* front = data->m_records.front().get();

	if (front && (front->broke_lc() || front->m_sim_time < front->m_old_sim_time))
		return nullptr;

	// set this
	bool found_last = false;

	// iterate records in reverse.
	for (auto it = data->m_records.crbegin(); it != data->m_records.crend(); ++it) {
		current = it->get();

		// lets go ahead and do like game and just stop lagcompensating if break lc
		if (current->broke_lc())
			break;

		// if this record is valid.
		// we are done since we iterated in reverse.
		if (current->valid() && !current->immune() && !current->dormant()) {

			// if we previous found a last, return current
			if (found_last)
				return current;

			// else that means this is an interpolated record
			// so we dont wanna use this to backtrack, lets go ahead and skip it
			found_last = true;
		}
	}

	return nullptr;
}

void Resolver::OnBodyUpdate(Player* player, float value) {

}



float Resolver::GetAwayAngle(LagRecord* record) {


	int nearest_idx = GetNearestEntity(record->m_player, record);
	Player* nearest = (Player*)g_csgo.m_entlist->GetClientEntity(nearest_idx);

	if (!nearest)
		return 0.f;

	ang_t  away;
	math::VectorAngles(nearest->m_vecOrigin() - record->m_pred_origin, away);
	return away.y;
}



void Resolver::MatchShot(AimPlayer* data, LagRecord* record) {

	Weapon* wpn = data->m_player->GetActiveWeapon();

	if (!wpn)
		return;

	WeaponInfo* wpn_data = wpn->GetWpnData();

	if (!wpn_data)
		return;

	if ((wpn_data->m_weapon_type != WEAPONTYPE_GRENADE && wpn_data->m_weapon_type > 6) || wpn_data->m_weapon_type <= 0)
		return;

	const auto shot_time = wpn->m_fLastShotTime();
	const auto shot_tick = game::TIME_TO_TICKS(shot_time);

	if (shot_tick == game::TIME_TO_TICKS(record->m_sim_time) && record->m_lag <= 2)
		record->m_shot_type = 2;
	else {
		bool should_correct_pitch = false;

		if (shot_tick == game::TIME_TO_TICKS(record->m_anim_time)) {
			record->m_shot_type = 1;
			should_correct_pitch = true;
		}
		else if (shot_tick >= game::TIME_TO_TICKS(record->m_anim_time)) {
			if (shot_tick <= game::TIME_TO_TICKS(record->m_sim_time))
				should_correct_pitch = true;
		}

		if (should_correct_pitch) {
			float valid_pitch = 89.f;

			for (const auto& it : data->m_records) {
				if (it.get() == record || it->dormant() || it->immune())
					continue;

				if (it->m_shot_type <= 0) {
					valid_pitch = it->m_eye_angles.x;
					break;
				}
			}

			record->m_eye_angles.x = valid_pitch;
		}
	}

	if (record->m_shot_type > 0)
		record->m_resolver_mode = "SHOT";
}

void Resolver::SetMode(LagRecord* record) {

	// the resolver has 3 modes to chose from.
	// these modes will vary more under the hood depending on what data we have about the player
	// and what kind of hack vs. hack we are playing (mm/nospread).
	float speed = record->m_velocity.length_2d();
	const int flags = record->m_broke_lc ? record->m_pred_flags : record->m_player->m_fFlags();
	// check if they've been on ground for two consecutive ticks.
	if (flags & FL_ONGROUND) {

		// ghetto fix for fakeflick
		if (speed <= 35.f && g_input.GetKeyState(g_menu.main.aimbot.override.get()))
			record->m_mode = Modes::RESOLVE_OVERRIDE;
		// stand resolver if not moving or fakewalking
		else if (speed <= 0.1f || record->m_fake_walk)
			record->m_mode = Modes::RESOLVE_STAND;
		// moving resolver at the end if none above triggered
		else
			record->m_mode = Modes::RESOLVE_WALK;
	}
	// if not on ground.
	else
		record->m_mode = Modes::RESOLVE_AIR;
}


bool Resolver::IsSideways(float angle, LagRecord* record) {

	ang_t  away;
	math::VectorAngles(g_cl.m_shoot_pos - record->m_pred_origin, away);
	const float diff = math::AngleDiff(away.y, angle);
	return diff > 45.f && diff < 135.f;
}


void Resolver::ResolveAngles(Player* player, LagRecord* record) {

	if (record->m_weapon) {
		WeaponInfo* wpn_data = record->m_weapon->GetWpnData();
		if (wpn_data && wpn_data->m_weapon_type == WEAPONTYPE_GRENADE) {
			if (record->m_weapon->m_bPinPulled()
				&& record->m_weapon->m_fThrowTime() > 0.0f) {
				record->m_resolver_mode = "PIN";
				return;
			}
		}
	}

	if (player->m_MoveType() == MOVETYPE_LADDER || player->m_MoveType() == MOVETYPE_NOCLIP) {
		record->m_resolver_mode = "LADDER";
		return;
	}


	AimPlayer* data = &g_aimbot.m_players[player->index() - 1];


	// mark this record if it contains a shot.
	MatchShot(data, record);


	if (data->m_last_stored_body == FLT_MIN)
		data->m_last_stored_body = record->m_body;

	if (record->m_velocity.length_2d() > 0.1f && (record->m_flags & FL_ONGROUND)) {
		data->m_has_ever_updated = false;
		data->m_last_stored_body = record->m_body;
		data->m_change_stored = 0;
	}
	else if (std::fabs(math::AngleDiff(data->m_last_stored_body, record->m_body)) > 1.f
		&& record->m_shot_type <= 0) {
		data->m_has_ever_updated = true;
		data->m_last_stored_body = record->m_body;
		++data->m_change_stored;
	}


	if (data->m_records.size() >= 2 && record->m_shot_type <= 0) {
		LagRecord* previous = data->m_records[1].get();
		const float lby_delta = math::AngleDiff(record->m_body, previous->m_body);

		if (std::fabs(lby_delta) > 0.5f && !previous->m_dormant) {

			data->m_body_timer = FLT_MIN;
			data->m_body_updated_idk = 0;

			if (data->m_has_updated) {

				if (std::fabs(lby_delta) <= 155.f) {

					if (std::fabs(lby_delta) > 25.f) {

						if (record->m_flags & FL_ONGROUND) {

							if (std::fabs(record->m_anim_time - data->m_upd_time) < 1.5f)
								++data->m_update_count;

							data->m_upd_time = record->m_anim_time;
						}
					}
				}
				else {
					data->m_has_updated = 0;
					data->m_update_captured = 0;
				}
			}
			else if (std::fabs(lby_delta) > 25.f) {
				if (record->m_flags & FL_ONGROUND) {

					if (std::fabs(record->m_anim_time - data->m_upd_time) < 1.5f)
						++data->m_update_count;

					data->m_upd_time = record->m_anim_time;
				}
			}
		}
	}

	// set to none
	record->m_resolver_mode = "NONE";

	// next up mark this record with a resolver mode that will be used.
	SetMode(record);

	// 0 pitch correction
	if (record->m_mode != Modes::RESOLVE_WALK
		&& record->m_shot_type <= 0) {

		LagRecord* previous = data->m_records.size() >= 2 ? data->m_records[1].get() : nullptr;

		if (previous && !previous->dormant()) {

			const float yaw_diff = math::AngleDiff(previous->m_eye_angles.y, record->m_eye_angles.y);
			const float body_diff = math::AngleDiff(record->m_body, record->m_eye_angles.y);
			const float eye_diff = record->m_eye_angles.x - previous->m_eye_angles.x;

			if (std::abs(eye_diff) <= 35.f
				&& std::abs(record->m_eye_angles.x) <= 45.f
				&& std::abs(yaw_diff) <= 45.f) {
				record->m_resolver_mode = "PITCH 0";
				return;
			}
		}
	}

	switch (record->m_mode) {
	case Modes::RESOLVE_WALK:
		ResolveWalk(data, record);
		break;
	case Modes::RESOLVE_STAND:
		ResolveStand(data, record);
		break;
	case Modes::RESOLVE_AIR:
		ResolveAir(data, record, player);
		break;
	case Modes::RESOLVE_OVERRIDE:
		ResolveOverride(data, record, record->m_player);
		break;
	default:
		break;
	}

	if (data->m_old_stand_move_idx != data->m_stand_move_idx
		|| data->m_old_stand_no_move_idx != data->m_stand_no_move_idx) {
		data->m_old_stand_move_idx = data->m_stand_move_idx;
		data->m_old_stand_no_move_idx = data->m_stand_no_move_idx;

		if (auto animstate = player->m_PlayerAnimState(); animstate != nullptr) {
			animstate->m_foot_yaw = record->m_eye_angles.y;
			player->SetAbsAngles(ang_t{ 0, animstate->m_foot_yaw, 0 });
		}
	}

	// normalize the eye angles, doesn't really matter but its clean.
	math::NormalizeAngle(record->m_eye_angles.y);
}

void Resolver::ResolveAir(AimPlayer* data, LagRecord* record, Player* player) {
	

	float velyaw = math::rad_to_deg(std::atan2(record->m_velocity.y, record->m_velocity.x));

	LagRecord* move = &data->m_walk_record;
	LagRecord* previous = data->m_records.size() > 1 ? data->m_records[1].get()->dormant() ? nullptr : data->m_records[1].get() : nullptr;

	iPlayers[player->index()] = true;

	float back_diff = fabsf(velyaw + 180.0f - record->m_body);

	const Stage_t stage{};
	if (stage != FRAME_NET_UPDATE_POSTDATAUPDATE_START)
		return;

	for (auto i = 1; i < g_csgo.m_globals->m_max_clients; i++)
	{
		if (player || g_csgo.IsLocalPlayer)
			continue;

		if (player->m_flSimulationTime() <= player->m_flOldSimulationTime())
			continue;

		if (!player->alive())
			continue;

		if (player->dormant())
			continue;

		const auto simulation_tick_delta = game::TICKS_TO_TIME(player->m_flSimulationTime() - player->m_flOldSimulationTime());
		if (simulation_tick_delta > 15 && simulation_tick_delta < 2)
			return;

		auto in_air = false;
		if (!(player->m_fFlags() & FL_ONGROUND) || !(record->m_pred_flags & FL_ONGROUND))
			in_air = true;
		auto ticks_left = static_cast<int>(simulation_tick_delta);
		ticks_left = (ticks_left, 0, 10);
		while (ticks_left > 0)
		{
			auto data_origin = player->m_vecOrigin();
			auto data_velocity = player->m_vecAbsVelocity();
			auto data_flags = player->m_fFlags();

			player->m_flSimulationTime() += record->m_anim_time;
			data_origin;
			player->m_vecAbsVelocity() = data_velocity;
			player->m_fFlags() = data_flags;
			--ticks_left;
		}
	}
	record->m_resolver_mode = XOR("air");
	record->m_resolver_mode = "air";

	if (record->m_velocity.length_2d() < 60.f) {
		record->m_mode = Modes::RESOLVE_STAND;

		// stand and getaway resolver
		ResolveStand(data, record);
		GetAwayAngle(record);
		record->m_resolver_mode = "AIR";
		return;
	}

	record->m_mode = Modes::RESOLVE_AIR;
	bool can_last_move_air = move->m_anim_time > 0.f &&
		fabsf(move->m_body - record->m_body) < 12.5f
		&& data->m_air_brute_index < 1;

	if (back_diff <= 18.f && data->m_air_brute_index < 2) {
		record->m_resolver_mode = XOR("AIR-BACK");
		record->m_resolver_color = colors::transparent_green;
		record->m_eye_angles.y = velyaw + 180.f;
	}
	else if (can_last_move_air)
	{
		record->m_resolver_mode = XOR("AIR-MOVE");
		record->m_resolver_color = colors::transparent_green;
		record->m_eye_angles.y = move->m_body;
	}
	else
	{
		switch (data->m_air_brute_index % 3) {
		case 0:
			record->m_resolver_mode = XOR("BAIR-BACK");
			record->m_resolver_color = colors::transparent_green;
			record->m_eye_angles.y = velyaw + 180.f;
			break;
		case 1:
			record->m_resolver_mode = XOR("BAIR-LBYPOS");
			record->m_resolver_color = colors::red;
			record->m_eye_angles.y = record->m_body + 35.f;
			break;
		case 2:
			record->m_resolver_mode = XOR("BAIR-LBYNEG");
			record->m_resolver_color = colors::red;
			record->m_eye_angles.y = record->m_body - 35.f;
			break;
		default:
			break;
		}

		float away = GetAwayAngle(record);
		const float flVelocityDirYaw = math::rad_to_deg(std::atan2(player->m_vecVelocity().x, player->m_vecVelocity().y));

		switch (data->m_air_index % 3) {
		case 0:
			record->m_eye_angles.y = record->m_player->m_flLowerBodyYawTarget();
			record->m_resolver_mode = "AIR:KAABA:1";
			record->m_resolver_color = colors::orange;
			break;
		case 1:
			if (move->m_body < FLT_MAX && abs(math::AngleDiff(player->m_flLowerBodyYawTarget(), move->m_body)) > 60.f) {
				record->m_resolver_mode = "AIR:KAABA:2:LM";
				record->m_resolver_color = colors::orange;
				record->m_eye_angles.y = move->m_body;
			}
			else {
				record->m_eye_angles.y = away + 180.f;
				record->m_resolver_mode = "AIR:KAABA:2:AWAY";
				record->m_resolver_color = colors::orange;
			}
			break;
		case 2:
			record->m_resolver_mode = "AIR:KAABA:AWAY";
			record->m_resolver_color = colors::orange;
			record->m_eye_angles.y = away;
			break;
		case 3:
			record->m_resolver_mode = "AIR:KAABA:VELYAW";
			record->m_resolver_color = colors::orange;
			record->m_eye_angles.y = flVelocityDirYaw - 180.f;
			break;
		}
	}
}

void Resolver::ResolveWalk(AimPlayer* data, LagRecord* record) {
	// apply lby to eyeangles.
	record->m_eye_angles.y = record->m_body;

	// reset stand and body index.

	data->m_body_timer = record->m_anim_time + 0.22f;
	data->m_body_updated_idk = 0;
	data->m_update_captured = 0;
	data->m_has_updated = 0;
	data->m_last_body = FLT_MIN;
	data->m_overlap_offset = 0.f;

	const float speed_2d = record->m_velocity.length_2d();

	if (speed_2d > record->m_max_speed * 0.34f) {
		data->m_update_count = 0;
		data->m_upd_time = FLT_MIN;
		data->m_body_pred_idx
			= data->m_body_idx
			= data->m_old_stand_move_idx
			= data->m_old_stand_no_move_idx
			= data->m_stand_move_idx
			= data->m_stand_no_move_idx = 0;
		data->m_missed_back = data->m_missed_invertfs = false;
	}

	// copy the last record that this player was walking
	// we need it later on because it gives us crucial data.
	// copy move data over
	if (speed_2d > 25.f)
		data->m_walk_record.m_body = record->m_body;

	data->m_walk_record.m_origin = record->m_origin;
	data->m_walk_record.m_anim_time = record->m_anim_time;
	data->m_walk_record.m_sim_time = record->m_sim_time;

	record->m_resolver_mode = "WALK";
}


int Resolver::GetNearestEntity(Player* target, LagRecord* record) {

	// best data
	int idx = g_csgo.m_engine->GetLocalPlayer();
	float best_distance = g_cl.m_local && g_cl.m_processing ? g_cl.m_local->m_vecOrigin().dist_to(record->m_pred_origin) : FLT_MAX;

	// cur data
	Player* curr_player = nullptr;
	vec3_t  curr_origin{ };
	float   curr_dist = 0.f;
	AimPlayer* data = nullptr;

	for (int i{ 1 }; i <= g_csgo.m_globals->m_max_clients; ++i) {
		curr_player = g_csgo.m_entlist->GetClientEntity< Player* >(i);

		if (!curr_player
			|| !curr_player->IsPlayer()
			|| curr_player->index() > 64
			|| curr_player->index() <= 0
			|| !curr_player->enemy(target)
			|| curr_player->dormant()
			|| !curr_player->alive()
			|| curr_player == target)
			continue;

		curr_origin = curr_player->m_vecOrigin();
		curr_dist = record->m_pred_origin.dist_to(curr_origin);

		if (curr_dist < best_distance) {
			idx = i;
			best_distance = curr_dist;
		}
	}

	return idx;
}

void Resolver::ResolveStand(AimPlayer* data, LagRecord* record) {

	int idx = GetNearestEntity(record->m_player, record);
	Player* nearest_entity = (Player*)g_csgo.m_entlist->GetClientEntity(idx);

	if (!nearest_entity)
		return;

	if ((data->m_is_cheese_crack || data->m_is_kaaba) && data->m_network_index <= 1) {
		record->m_eye_angles.y = data->m_networked_angle;
		record->m_resolver_color = { 155, 210, 100 };
		record->m_resolver_mode = "NETWORKED";
		record->m_mode = Modes::RESOLVE_NETWORK;
		return;
	}

	const float away = GetAwayAngle(record);

	data->m_moved = false;

	if (data->m_walk_record.m_sim_time > 0.f) {

		vec3_t delta = data->m_walk_record.m_origin - record->m_origin;

		// check if moving record is close.
		if (delta.length() <= 32.f) {
			// indicate that we are using the moving lby.
			data->m_moved = true;
		}
	}

	const float back = away + 180.f;

	record->m_back = back;


	bool updated_body_values = false;
	const float move_lby_diff = math::AngleDiff(data->m_walk_record.m_body, record->m_body);
	const float forward_body_diff = math::AngleDiff(away, record->m_body);
	const float time_since_moving = record->m_anim_time - data->m_walk_record.m_anim_time;
	const float override_backwards = 50.f;

	if (record->m_anim_time > data->m_body_timer) {

		if (data->m_player->m_fFlags() & FL_ONGROUND) {
			updated_body_values = 1;

			if (!data->m_update_captured && data->m_body_timer != FLT_MIN) {
				data->m_has_updated = 1;
				updated_body_values = 0;
			}

			if (record->m_shot_type == 1) {
				if (!data->m_update_captured) {
					data->m_update_captured = true;
					data->m_second_delta = 0.f;
				}
			}
			else if (updated_body_values)
				record->m_eye_angles.y = record->m_body;


			if (data->m_update_captured) {

				const int sequence_activity = data->m_player->GetSequenceActivity(record->m_layers[3].m_sequence);
				if (!data->m_moved
					|| data->m_has_updated
					|| std::fabs(data->m_second_delta) > 35.f
					|| std::fabs(move_lby_diff) <= 90.f) {

					if (sequence_activity == 979
						&& record->m_layers[3].m_cycle == 0.f
						&& record->m_layers[3].m_weight == 0.f) {
						data->m_second_delta = std::fabs(data->m_second_delta);
						data->m_first_delta = std::fabs(data->m_first_delta);
					}
					else {
						data->m_second_delta = -std::fabs(data->m_second_delta);
						data->m_first_delta = -std::fabs(data->m_first_delta);
					}
				}
				else {
					data->m_first_delta = move_lby_diff;
					data->m_second_delta = move_lby_diff;
				}
			}
			else {

				if (data->m_walk_record.m_sim_time <= 0.f
					|| data->m_walk_record.m_anim_time <= 0.f) {
					data->m_second_delta = data->m_first_delta;
					data->m_last_body = FLT_MIN;
				}
				else {
					data->m_first_delta = move_lby_diff;
					data->m_second_delta = move_lby_diff;
					data->m_last_body = std::fabs(move_lby_diff - 90.f) <= 10.f ? FLT_MIN : record->m_body;
				}

				data->m_update_captured = true;
			}

			if (updated_body_values && data->m_body_pred_idx <= 0) {
				data->m_body_timer = record->m_anim_time + 1.1f;
				record->m_mode = Modes::RESOLVE_LBY_PRED;
				record->m_resolver_mode = "LBYPRED";
				return;
			}
		}
	}


	// reset overlap delta amount
	data->m_overlap_offset = 0.f;

	// just testing
	if (g_menu.main.aimbot.correct_opt.get(0)) {
		const float back_delta = math::AngleDiff(record->m_body, back);
		if (std::fabs(back_delta) >= 15.f) {
			if (back_delta < 0.f) {
				data->m_overlap_offset = std::clamp(-(std::fabs(back_delta) / 2.f), -35.f, 35.f);
				record->m_resolver_mode = "F:OVERLAP-";
			}
			else {
				data->m_overlap_offset = std::clamp((std::fabs(back_delta) / 2.f), -35.f, 35.f);
				record->m_resolver_mode = "F:OVERLAP+";
			}
		}
	}

	const int   balance_adj_act = data->m_player->GetSequenceActivity(record->m_layers[3].m_sequence); // layer 3 is balance adjust
	const float min_body_yaw = 30.f; // was a menu item
	const vec3_t current_origin = record->m_origin + record->m_player->m_vecViewOffset();
	const vec3_t nearest_origin = nearest_entity->m_vecOrigin() + nearest_entity->m_vecViewOffset();
	if (record->m_shot_type != 1) {
		if (g_menu.main.aimbot.correct_opt.get(1)) {
			if (time_since_moving > 0.0f
				&& time_since_moving <= 0.22f
				&& data->m_body_idx <= 0) {
				record->m_eye_angles.y = record->m_body;
				record->m_mode = Modes::RESOLVE_LBY;
				record->m_resolver_mode = "SM:LBY";
				return;
			}

			if (data->m_update_count <= 0
				&& data->m_body_idx <= 0
				&& data->m_change_stored <= 1) {

				record->m_eye_angles.y = record->m_body;
				record->m_mode = Modes::RESOLVE_LBY;
				record->m_resolver_mode = "HN:LBY";

				// extra safety
				if (data->m_moved && std::abs(math::AngleDiff(record->m_body, data->m_walk_record.m_body)) <= 90.f
					|| !data->m_moved)
					return;
			}
		}

		if (!data->m_moved) {

			record->m_mode = Modes::RESOLVE_NO_DATA;

			// we need it cus this reso is retarded
			if (data->m_stand_no_move_idx >= 3)
				data->m_stand_no_move_idx = 0;

			const int missed_no_data = data->m_stand_no_move_idx;



			if (missed_no_data) {
				if (missed_no_data == 1) {

					if (std::fabs(data->m_first_delta) > min_body_yaw) {
						record->m_resolver_mode = "S:BACK";
						record->m_eye_angles.y = back + data->m_overlap_offset;
					}
					else {
						record->m_resolver_mode = data->m_has_updated ? "S:LBYFS" : "S:LBY";
						record->m_eye_angles.y = data->m_has_updated ? AntiFreestand(record->m_player, record, nearest_origin, current_origin, true, record->m_body, 65.f) : record->m_body;
					}

					return;
				}

				if (missed_no_data != 2) {
					record->m_resolver_mode = "S:CANCER";
					record->m_eye_angles.y = back;
					return;
				}

				if (std::fabs(data->m_first_delta) <= min_body_yaw) {
					record->m_resolver_mode = "S:BACK2";
					record->m_eye_angles.y = back;
					return;
				}

				if (override_backwards >= std::fabs(forward_body_diff)
					|| (std::fabs(forward_body_diff) >= (180.f - override_backwards))) {
					record->m_resolver_mode = "S:LBYDELTA";
					record->m_eye_angles.y = record->m_body + data->m_first_delta;
					return;
				}
			}
			else {

				if (std::fabs(data->m_first_delta) <= min_body_yaw) {
					const bool body = data->m_has_updated && data->m_update_count <= 1;
					record->m_resolver_mode = body ? "S:LBY2" : "S:LBYFS2";
					record->m_eye_angles.y = body ? record->m_body : AntiFreestand(record->m_player, record, nearest_origin, current_origin, true, record->m_body, 65.f);
					return;
				}

				if (data->m_update_count <= 2) {
					if (override_backwards < std::fabs(forward_body_diff)
						&& (std::fabs(forward_body_diff) < (180.f - override_backwards))) {

						const float lby_test2_neg = record->m_body - std::fabs(data->m_first_delta);
						const float lby_test2_pos = record->m_body + std::fabs(data->m_first_delta);
						const float diff_pos = std::fabs(math::AngleDiff(back, lby_test2_pos));
						const float diff_neg = std::fabs(math::AngleDiff(back, lby_test2_neg));

						record->m_resolver_mode = "S:DELTA-BACK";
						record->m_eye_angles.y = diff_pos < diff_neg ? lby_test2_pos : lby_test2_neg;
						return;
					}

					record->m_resolver_mode = "S:DELTA-FS";
					record->m_eye_angles.y = AntiFreestand(record->m_player, record, nearest_origin, current_origin, false, record->m_body, std::clamp(std::fabs(data->m_first_delta), 35.f, 65.f));
					return;
				}
			}

			record->m_resolver_mode = "S:INVERT-FS";
			record->m_eye_angles.y = AntiFreestand(record->m_player, record, nearest_origin, current_origin, false, away + 180.f, 90.f);
			return;
		}

		record->m_resolver_mode = "M:";
		record->m_mode = Modes::RESOLVE_DATA;


		float delta_case_1 = FLT_MIN;
		constexpr float max_last_lby_diff = 10.f;
		const float last_lby_diff = math::AngleDiff(data->m_last_body, record->m_body);
		const float move_back_diff = math::AngleDiff(back, data->m_walk_record.m_body);
		const float lby_back_diff = math::AngleDiff(back, record->m_body);

		bool been_in_case0 = false;

		// this is so cancer
		if (data->m_stand_move_idx > 5)
			data->m_stand_move_idx = 0;

		switch (data->m_stand_move_idx % 5) {
		case 0:
			been_in_case0 = true;

			if (std::fabs(data->m_second_delta) > min_body_yaw) {

				if (data->m_last_body == FLT_MIN
					|| data->m_change_stored >= 2
					|| data->m_update_count > 3
					|| (data->m_body_idx > 0 && std::abs(move_lby_diff) <= 22.5f))
					goto SKIP_LASTDIFF;

				if (std::fabs(last_lby_diff) >= max_last_lby_diff)
					goto SKIP_LASTDIFF;

				record->m_resolver_mode += "LAST-DIFF";
				record->m_eye_angles.y = data->m_walk_record.m_body + std::clamp(last_lby_diff, -35.f, 35.f);
				record->m_has_seen_delta_35 = 1;
			}
			else {
				record->m_resolver_mode += data->m_has_updated
					&& data->m_update_count <= 1
					&& data->m_body_idx <= 0 ? "LBY" : "LAST";
				record->m_eye_angles.y = data->m_has_updated
					&& data->m_update_count <= 1
					&& data->m_body_idx <= 0 ? record->m_body : data->m_walk_record.m_body;
			}

			break;
		case 1:
			if (std::fabs(data->m_second_delta) > min_body_yaw) {

				if (data->m_last_body == FLT_MIN
					|| std::fabs(last_lby_diff) >= max_last_lby_diff) {
					record->m_eye_angles.y = data->m_walk_record.m_body;
					record->m_resolver_mode += "LAST2";
				}
				else {
				SKIP_LASTDIFF:

					// we havent missed yet
					// and his lastmove is near his backward angle
					// and his lby only updated once
					if (been_in_case0
						&& std::abs(move_back_diff) <= 50.f
						&& data->m_update_count <= 1
						&& data->m_change_stored <= 2) {


						const float diff = data->m_update_count == 0 ? lby_back_diff : move_back_diff;
						record->m_resolver_mode += "LASTBACK";

						// we have a delta that's high enough to hit an overlap
						if (std::abs(diff) > 12.5f) {

							// get opposites angles
							const float move_back_neg = back - (std::abs(diff) / 2.f);
							const float move_back_pos = back + (std::abs(diff) / 2.f);

							// get delta between back and the opposite angles
							const float diff_pos = std::fabs(math::AngleDiff(back, move_back_pos));
							const float diff_neg = std::fabs(math::AngleDiff(back, move_back_neg));

							// overlap back with the nearest angle
							record->m_resolver_mode += ":1";
							record->m_eye_angles.y = diff_pos < diff_neg ? move_back_pos : move_back_neg;
						}
						// or delta is too low to hit one
						else {
							record->m_resolver_mode += ":0";
							record->m_eye_angles.y = back;
						}

					}
					else if (data->m_update_count <= 2) {
						if (data->m_update_captured) {
							record->m_resolver_mode += "2:";
							delta_case_1 = std::fabs(data->m_second_delta);
						}
						else {
							record->m_resolver_mode += "1:";
							delta_case_1 = std::fabs(data->m_first_delta);
						}

						if (data->m_update_captured
							&& std::fabs(data->m_second_delta) > 135.f // 150.f
							&& data->m_change_stored <= 1) { // test so it doesnt put head out ig
							record->m_resolver_mode += "LBY-DELTA";
							record->m_eye_angles.y = record->m_body + data->m_second_delta;
							record->m_has_seen_delta_35 = data->m_stand_move_idx == 0;
						}
						else {

							if (override_backwards < std::fabs(forward_body_diff)
								&& std::fabs(forward_body_diff) < (180.f - override_backwards)) {
								const float lby_back_neg = record->m_body - delta_case_1;
								const float lby_back_pos = record->m_body + delta_case_1;

								const float diff_pos = std::fabs(math::AngleDiff(back, lby_back_pos));
								const float diff_neg = std::fabs(math::AngleDiff(back, lby_back_neg));


								record->m_resolver_mode += "LBYBACK";
								record->m_eye_angles.y = diff_pos < diff_neg ? lby_back_pos : lby_back_neg;
								return;
							}

							record->m_resolver_mode += "LBY-FSDELTA";
							record->m_eye_angles.y = AntiFreestand(record->m_player, record, nearest_origin, current_origin, false, record->m_body, std::clamp(delta_case_1, 35.f, 65.f));
							record->m_has_seen_delta_35 = data->m_stand_move_idx == 0;
						}
					}
					else {

						// should only happen if we missed 5 shots so
						if (data->m_missed_invertfs && data->m_missed_back) {
							record->m_eye_angles.y = data->m_walk_record.m_body;
							record->m_resolver_mode += "LAST3";
						}
						else if (data->m_missed_invertfs) {

							if (std::abs(move_back_diff) <= 22.5f) {
								record->m_eye_angles.y = back;
								record->m_resolver_mode += "LASTBACK2";
							}
							else {
								record->m_eye_angles.y = AntiFreestand(record->m_player, record, nearest_origin, current_origin, true, away + 180.f, 45.f);
								record->m_resolver_mode += "INVERTFS2";
							}
						}
						else {
							record->m_eye_angles.y = AntiFreestand(record->m_player, record, nearest_origin, current_origin, true, away + 180.f, 90.f);
							record->m_resolver_mode += "INVERTFS";
						}
					}
				}
			}
			else {
				record->m_resolver_mode += data->m_has_updated && data->m_body_idx <= 0 ? "LBY3" : "LAST3";
				// record->m_eye_angles.y = data->m_has_updated ? record->m_body : data->m_walk_record.m_body;
				record->m_eye_angles.y = AntiFreestand(record->m_player, record, nearest_origin, current_origin, true, data->m_has_updated ? record->m_body : data->m_walk_record.m_body, 65.f);
			}

			break;
		case 2:
			if (std::fabs(data->m_second_delta) > min_body_yaw) {

				if (data->m_update_captured && std::fabs(data->m_second_delta) <= 135.f) { // 150.f
					record->m_resolver_mode += "LBY-DELTA2";
					record->m_eye_angles.y = record->m_body + data->m_second_delta;
				}
				else {
					record->m_resolver_mode += "BACK";
					record->m_eye_angles.y = back + data->m_overlap_offset;
				}
			}
			else {
				record->m_resolver_mode += "LBY-INVERTFS";
				record->m_eye_angles.y = AntiFreestand(record->m_player, record, nearest_origin, current_origin, true, record->m_body, 90.f);
			}
			break;
		case 3:
			record->m_resolver_mode += "BACK2";
			record->m_eye_angles.y = back;
			break;
		case 4:
			record->m_resolver_mode += "FRONT";
			record->m_eye_angles.y = away;
			break;
		default:
			break;
		}
	}
	else
		record->m_resolver_mode = "S:SHOT";
}

void Resolver::ResolveOverride(AimPlayer* data, LagRecord* record, Player* player) {
	// get predicted away angle for the player.
	float away = GetAwayAngle(record);

	C_AnimationLayer* curr = &record->m_layers[3];
	int act = data->m_player->GetSequenceActivity(curr->m_sequence);


	record->m_resolver_mode = "OVERRIDE";
	ang_t                          viewangles;
	g_csgo.m_engine->GetViewAngles(viewangles);

	//auto yaw = math::clamp (g_cl.m_local->GetAbsOrigin(), Player->origin()).y;
	const float at_target_yaw = math::CalcAngle(g_cl.m_local->m_vecOrigin(), player->m_vecOrigin()).y;
	const float dist = math::NormalizedAngle(viewangles.y - at_target_yaw);

	float brute = 0.f;

	if (std::abs(dist) <= 1.f) {
		brute = at_target_yaw;
		record->m_resolver_mode += ":BACK";
	}
	else if (dist > 0) {
		brute = at_target_yaw + 90.f;
		record->m_resolver_mode += ":RIGHT";
	}
	else {
		brute = at_target_yaw - 90.f;
		record->m_resolver_mode += ":LEFT";
	}

	record->m_eye_angles.y = brute;


}

#pragma optimize( "", on )