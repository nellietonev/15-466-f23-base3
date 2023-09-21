#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <stdlib.h>

#include <random>

GLuint level1_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > level1_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("level1.pnct"));
	level1_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > level1_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("level1.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = level1_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = level1_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;
	});
});

void PlayMode::GeneratePlatforms(bool is_initial_drawing, Direction new_direction, float vertical_offset) {
	for (size_t i = 0; i < row_size; i++) {
		/* Lower row -- draw only if it is the first row or if direction is changing */
		if ((is_initial_drawing || new_direction != direction) && i != 0) {
			scene.transforms.emplace_back();
			Scene::Transform &transform = scene.transforms.back();
			transform.position = block_row_left_anchor + glm::vec3(0.0f, i, vertical_offset);

			scene.drawables.emplace_back(&transform);
			Scene::Drawable &drawable = scene.drawables.back();
			drawable.pipeline = block_pipeline;
		}
		/* upper row */
		{
			scene.transforms.emplace_back();
			Scene::Transform &transform = scene.transforms.back();
			transform.position = block_row_left_anchor + glm::vec3(0.0f, i, vertical_offset + 3.0f);

			scene.drawables.emplace_back(&transform);
			Scene::Drawable &drawable = scene.drawables.back();
			drawable.pipeline = block_pipeline;
		}
	}
}

void PlayMode::DetermineSoundsForEachBlock() {
	blocks_sound_vector.clear();
    uint8_t good_sound_index = rand() % row_size;
	for (size_t i = 0; i < row_size; i++) {
		blocks_sound_vector.push_back((i == good_sound_index) ? 1 : 0);
	}
}

void PlayMode::ResetPlayerPosition() {
	if (player == nullptr) return;
	player->position = player_reset_position;
	player->rotation = player_reset_rotation;
	player_block_index = 0;
}

PlayMode::PlayMode() : scene(*level1_scene) {
	for (auto &drawable : scene.drawables) {
		if (drawable.transform->name == "Player") player = drawable.transform;
		else if (drawable.transform->name == "Block") block_pipeline = drawable.pipeline;
	}

	if (player == nullptr) throw std::runtime_error("Player mesh not found.");

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
	camera->transform->position.z -= 1.0f; // adding a bit of camera offset

	row_size = 12;
	block_row_left_anchor = glm::uvec3(0.0f);

	/* initial player position, separate from what's in blender */
	player->position = block_base_transform.position + glm::vec3(0.0f, 0.0f, 1.0f);
	player_reset_position = player->position;
	player_reset_rotation = player->rotation;

	// draw in first 2 rows of platforms and set one block to have the good sound
	vertical_offset = 0.0f;
	GeneratePlatforms(true, South, vertical_offset);
	DetermineSoundsForEachBlock();
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_SPACE) {
			space.downs += 1;
			space.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_q) {
			set_current(nullptr);
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_SPACE) {
			space.pressed = false;
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {
	// move player
	{
		constexpr float player_speed_multiplier = 0.5f;
		constexpr float player_jump_multiplier = 0.3f;
		constexpr float distance_to_jump = 3.0f;

		// TODO: change this based on the direction you are facing
		if (player_moving_horizontally) {
			/* continue existing movement */
			player->position += (player_distance_to_move * (elapsed / player_speed_multiplier));

			/* allow for some epsilon/margin of not hitting the exact target position */
			if (glm::all(glm::epsilonEqual(player->position, player_horizontal_target, 0.1f))) {
				player_moving_horizontally = false;
				player_block_index = next_player_block_index;
				player->position = player_horizontal_target;
			}
		}
		/* Note: Y-axis is flipped in the initial screen orientation */
		else if (!player_moving_horizontally && left.pressed && !right.pressed && player_block_index > 0) {
			/* start movement to the left */
			player_moving_horizontally = true;
			player_horizontal_target = player->position + glm::vec3(0.0f, -1.0f * block_base_transform.scale.y, 0.0f);
			player_distance_to_move = player_horizontal_target - player->position;
			next_player_block_index = player_block_index - 1;
		}
		else if (!player_moving_horizontally && !player_jumping && !left.pressed && right.pressed && player_block_index < row_size) {
			/* start movement to the right */
			player_moving_horizontally = true;
			player_horizontal_target = player->position + glm::vec3(0.0f, block_base_transform.scale.y, 0.0f);
			player_distance_to_move = player_horizontal_target - player->position;
			next_player_block_index = player_block_index + 1;
		}

		// play the appropriate sound if the up button is clicked
		if (up.pressed && !player_moving_horizontally && !player_jumping && (current_sound_effect == nullptr || current_sound_effect->stopped)) {
			current_sound_effect = Sound::play((blocks_sound_vector[player_block_index] == 1) ? good_block_sound : bad_block_sound);
		}

		// logic for jumping to next platform
		if (player_jumping) {
			player->position.z += (3.0f * (elapsed / player_jump_multiplier));

			/* allow for some epsilon/margin of not hitting the exact target position */
			if (glm::epsilonEqual(player->position.z, target_player_height, 0.5f)) {
				player_jumping = false;

				if (blocks_sound_vector[player_block_index] == 1) {
					player->position.z = target_player_height;
					vertical_offset += 3.0f;
					player_reset_position += glm::vec3(0.0f, 0.0f, 3.0f);
					GeneratePlatforms(false, South, vertical_offset);
					DetermineSoundsForEachBlock();
					camera->transform->position.z = player->position.z - 0.5f;
					current_streak += 1;
					if (longest_streak < current_streak) longest_streak = current_streak;
				} else {
					Sound::play(oof_got_hit_sound);
					ResetPlayerPosition();
					current_streak = 0;
				}
			}
		}
		else if (space.pressed & !player_moving_horizontally && !player_jumping) {
			/* start jumping */
			player_jumping = true;
			target_player_height = player->position.z + distance_to_jump;
		}

	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
	space.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.55f, 0.5f, 0.35f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("A and D moves player, click W to play sound, and SPACE to jump",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		lines.draw_text("A and D moves player, click W to play sound, and SPACE to jump",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));

		lines.draw_text("Current Streak: " + std::to_string((int)current_streak),
						glm::vec3( -aspect + 0.1f * H, 1.75f + -1.0 + 0.1f * H, 0.0),
						glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
						glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		lines.draw_text("Current Streak: " + std::to_string((int)current_streak),
						glm::vec3(-aspect + 0.1f * H + ofs, 1.75f + -1.0 + 0.1f * H + ofs, 0.0),
						glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
						glm::u8vec4(0xff, 0xff, 0xff, 0x00));

		lines.draw_text("Longest Streak: " + std::to_string((int)longest_streak),
						glm::vec3( -aspect + 0.1f * H, 1.875f + -1.0 + 0.1f * H, 0.0),
						glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
						glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		lines.draw_text("Longest Streak: " + std::to_string((int)longest_streak),
						glm::vec3(-aspect + 0.1f * H + ofs, 1.875f + -1.0 + 0.1f * H + ofs, 0.0),
						glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
						glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
	GL_ERRORS();
}
