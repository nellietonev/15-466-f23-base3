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

Load< Sound::Sample > dusty_floor_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("dusty-floor.opus"));
});

void PlayMode::GeneratePlatforms(bool is_initial_drawing, Direction new_direction) {
	for (size_t i = 0; i < row_size; i++) {
		/* Lower row -- draw only if it is the first row or if direction is changing */
		if ((is_initial_drawing || new_direction != direction) && i != 0) {
			scene.transforms.emplace_back();
			Scene::Transform &transform = scene.transforms.back();
			transform.position = block_row_left_anchor + glm::vec3(0.0f, i, 0.0f);

			scene.drawables.emplace_back(&transform);
			Scene::Drawable &drawable = scene.drawables.back();
			drawable.pipeline = block_pipeline;
		}
		/* upper row */
		{
			scene.transforms.emplace_back();
			Scene::Transform &transform = scene.transforms.back();
			transform.position = block_row_left_anchor + glm::vec3(0.0f, i, 3.0f);

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
		blocks_sound_vector.push_back(i == good_sound_index ? 1 : 0);
	}
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

	row_size = 12;
	block_row_left_anchor = glm::uvec3(0.0f);

	/* initial player position, separate from what's in blender */
	player->position = glm::uvec3(0.5f, 0.5f, 1.5f);

	// draw in first 2 rows of platforms and set one block to have the good sound
	GeneratePlatforms(true, South);
	DetermineSoundsForEachBlock();
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			player_moving_horizontally = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			player_moving_horizontally = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
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
		}
	} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			return true;
		}
	} else if (evt.type == SDL_MOUSEMOTION) {
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			camera->transform->rotation = glm::normalize(
				camera->transform->rotation
				* glm::angleAxis(-motion.x * camera->fovy, glm::vec3(0.0f, 1.0f, 0.0f))
				* glm::angleAxis(motion.y * camera->fovy, glm::vec3(1.0f, 0.0f, 0.0f))
			);
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {
	// move player
	{
		//combine inputs into a move:
		constexpr float PlayerSpeed = 5.0f;
		glm::vec3 move = glm::vec3(0.0f);

		// TODO: change this based on the direction you are facing
		if (left.pressed && !right.pressed) move.y = -1.0f;
		if (!left.pressed && right.pressed) move.y = 1.0f;
		if (up.pressed && (current_sound_effect == nullptr || current_sound_effect->stopped)) {
			std::cout << "the current player sound vector is " << blocks_sound_vector[player_block_index];
			current_sound_effect = Sound::play((blocks_sound_vector[player_block_index] == 1) ? good_block_sound : bad_block_sound);
		}
//		if (down.pressed && !up.pressed) move.y =-1.0f;
//		if (!down.pressed && up.pressed) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec3(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

		player->position += move;
	}

	{ //update listener to camera position:
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 frame_right = frame[0];
		glm::vec3 frame_at = frame[3];
		Sound::listener.set_position_right(frame_at, frame_right, 1.0f / 60.0f);
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
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
		lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));

		// TODO: update to actually have the correct number instead of hardcoded
		lines.draw_text("Current Streak: 0",
						glm::vec3( -aspect + 0.1f * H, 1.75f + -1.0 + 0.1f * H, 0.0),
						glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
						glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		lines.draw_text("Current Streak: 0",
						glm::vec3(-aspect + 0.1f * H + ofs, 1.75f + -1.0 + 0.1f * H + ofs, 0.0),
						glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
						glm::u8vec4(0xff, 0xff, 0xff, 0x00));

		lines.draw_text("Longest Streak: 0",
						glm::vec3( -aspect + 0.1f * H, 1.875f + -1.0 + 0.1f * H, 0.0),
						glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
						glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		lines.draw_text("Longest Streak: 0",
						glm::vec3(-aspect + 0.1f * H + ofs, 1.875f + -1.0 + 0.1f * H + ofs, 0.0),
						glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
						glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
	GL_ERRORS();
}

//glm::vec3 PlayMode::get_leg_tip_position() {
//	//the vertex position here was read from the model in blender:
//	return lower_leg->make_local_to_world() * glm::vec4(-1.26137f, -11.861f, 0.0f, 1.0f);
//}
