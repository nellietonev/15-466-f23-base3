#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"
#include "Load.hpp"

#include <glm/glm.hpp>
#include "data_path.hpp"

#include <vector>
#include <deque>
#include <array>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	/* used to determine how to move player and generate blocks */
	enum Direction : size_t {
		South = 0,
		East = 1,
		North = 2,
		West = 3,
	};

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	Scene::Drawable::Pipeline block_pipeline;
	Scene::Transform block_base_transform;
	Scene::Transform *player = nullptr;

	// direction of the environment determines where platforms are spawned and direction player can move
	Direction direction = South;

	//sound effects:
	Sound::Sample good_block_sound = Sound::Sample(data_path("good-block.wav"));
	Sound::Sample bad_block_sound = Sound::Sample(data_path("bad-block.wav"));
	Sound::Sample oof_got_hit_sound = Sound::Sample(data_path("oof.wav"));
	
	//camera:
	Scene::Camera *camera = nullptr;

	//game mechanic-related values:
	bool player_moving_horizontally;
	bool player_jumping;
	std::vector<uint8_t> blocks_sound_vector;

	glm::vec3 block_row_left_anchor;
	uint8_t row_size;

	// game helper functions:
	void DrawPlatforms();
};
