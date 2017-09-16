#include "load_save_png.hpp"
#include "GL.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <chrono>
#include <iostream>
#include <utility>
#include <stdexcept>

// macros
#define TILE_SIZE 128
#define TILE_X 5
#define TILE_Y 6
#define WINDOW_WIDTH (TILE_SIZE * 5)
#define WINDOW_HEIGHT (TILE_SIZE * (TILE_Y + 1))
#define ROCKS 5

static GLuint compile_shader(GLenum type, std::string const &source);
static GLuint link_program(GLuint vertex_shader, GLuint fragment_shader);

int main(int argc, char **argv) {
	//Configuration:
	struct {
		std::string title = "Game1: Text/Tiles";
        glm::uvec2 size = glm::uvec2(WINDOW_WIDTH, WINDOW_HEIGHT);
	} config;

	//------------  initialization ------------

	//Initialize SDL library:
	SDL_Init(SDL_INIT_VIDEO);

	//Ask for an OpenGL context version 3.3, core profile, enable debug:
	SDL_GL_ResetAttributes();
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

	//create window:
	SDL_Window *window = SDL_CreateWindow(
		config.title.c_str(),
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		config.size.x, config.size.y,
		SDL_WINDOW_OPENGL /*| SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI*/
	);

	if (!window) {
		std::cerr << "Error creating SDL window: " << SDL_GetError() << std::endl;
		return 1;
	}

	//Create OpenGL context:
	SDL_GLContext context = SDL_GL_CreateContext(window);

	if (!context) {
		SDL_DestroyWindow(window);
		std::cerr << "Error creating OpenGL context: " << SDL_GetError() << std::endl;
		return 1;
	}

	#ifdef _WIN32
	//On windows, load OpenGL extensions:
	if (!init_gl_shims()) {
		std::cerr << "ERROR: failed to initialize shims." << std::endl;
		return 1;
	}
	#endif

	//Set VSYNC + Late Swap (prevents crazy FPS):
	if (SDL_GL_SetSwapInterval(-1) != 0) {
		std::cerr << "NOTE: couldn't set vsync + late swap tearing (" << SDL_GetError() << ")." << std::endl;
		if (SDL_GL_SetSwapInterval(1) != 0) {
			std::cerr << "NOTE: couldn't set vsync (" << SDL_GetError() << ")." << std::endl;
		}
	}

	//Hide mouse cursor (note: showing can be useful for debugging):
	SDL_ShowCursor(SDL_DISABLE);

	//------------ opengl objects / game assets ------------

	//texture:
	GLuint tex = 0;
	glm::uvec2 tex_size = glm::uvec2(0,0);

	{ //load texture 'tex':
		std::vector< uint32_t > data;
		if (!load_png("spriteatlas.png", &tex_size.x, &tex_size.y, &data, LowerLeftOrigin)) {
			std::cerr << "Failed to load texture." << std::endl;
			exit(1);
		}
		//create a texture object:
		glGenTextures(1, &tex);
		//bind texture object to GL_TEXTURE_2D:
		glBindTexture(GL_TEXTURE_2D, tex);
		//upload texture data from data:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_size.x, tex_size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, &data[0]);
		//set texture sampling parameters:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	//shader program:
	GLuint program = 0;
	GLuint program_Position = 0;
	GLuint program_TexCoord = 0;
	GLuint program_Color = 0;
	GLuint program_mvp = 0;
	GLuint program_tex = 0;
	{ //compile shader program:
		GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER,
			"#version 330\n"
			"uniform mat4 mvp;\n"
			"in vec4 Position;\n"
			"in vec2 TexCoord;\n"
			"in vec4 Color;\n"
			"out vec2 texCoord;\n"
			"out vec4 color;\n"
			"void main() {\n"
			"	gl_Position = mvp * Position;\n"
			"	color = Color;\n"
			"	texCoord = TexCoord;\n"
			"}\n"
		);

		GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER,
			"#version 330\n"
			"uniform sampler2D tex;\n"
			"in vec4 color;\n"
			"in vec2 texCoord;\n"
			"out vec4 fragColor;\n"
			"void main() {\n"
			"	fragColor = texture(tex, texCoord) * color;\n"
			"}\n"
		);

		program = link_program(fragment_shader, vertex_shader);

		//look up attribute locations:
		program_Position = glGetAttribLocation(program, "Position");
		if (program_Position == -1U) throw std::runtime_error("no attribute named Position");
		program_TexCoord = glGetAttribLocation(program, "TexCoord");
		if (program_TexCoord == -1U) throw std::runtime_error("no attribute named TexCoord");
		program_Color = glGetAttribLocation(program, "Color");
		if (program_Color == -1U) throw std::runtime_error("no attribute named Color");

		//look up uniform locations:
		program_mvp = glGetUniformLocation(program, "mvp");
		if (program_mvp == -1U) throw std::runtime_error("no uniform named mvp");
		program_tex = glGetUniformLocation(program, "tex");
		if (program_tex == -1U) throw std::runtime_error("no uniform named tex");
	}

	//vertex buffer:
	GLuint buffer = 0;
	{ //create vertex buffer
		glGenBuffers(1, &buffer);
		glBindBuffer(GL_ARRAY_BUFFER, buffer);
	}

	struct Vertex {
		Vertex(glm::vec2 const &Position_, glm::vec2 const &TexCoord_, glm::u8vec4 const &Color_) :
			Position(Position_), TexCoord(TexCoord_), Color(Color_) { }
		glm::vec2 Position;
		glm::vec2 TexCoord;
		glm::u8vec4 Color;
	};
	static_assert(sizeof(Vertex) == 20, "Vertex is nicely packed.");

	//vertex array object:
	GLuint vao = 0;
	{ //create vao and set up binding:
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glVertexAttribPointer(program_Position, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLbyte *)0);
		glVertexAttribPointer(program_TexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLbyte *)0 + sizeof(glm::vec2));
		glVertexAttribPointer(program_Color, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (GLbyte *)0 + sizeof(glm::vec2) + sizeof(glm::vec2));
		glEnableVertexAttribArray(program_Position);
		glEnableVertexAttribArray(program_TexCoord);
		glEnableVertexAttribArray(program_Color);
	}

	//------------ sprite info ------------
	struct SpriteInfo {
		glm::vec2 min_uv;
		glm::vec2 max_uv;
		glm::vec2 rad;
	};

    /*
    auto load_sprite = [](std::string const &name) -> SpriteInfo {
        SpriteInfo info;
        //TODO: look up sprite name in table of sprite infos
        return info;
    };
    */

    SpriteInfo man;
    SpriteInfo map;
    SpriteInfo black_tile;
    SpriteInfo money_bag;
    SpriteInfo rock;
    SpriteInfo game_start;
    SpriteInfo game_end;
    SpriteInfo mine_with_space;
    
    //map
    map.min_uv = glm::vec2(0.0f, 0.0f);
    map.max_uv = glm::vec2(0.83333f, 0.88888f);
    map.rad = glm::vec2(0.5f);

    //black tile
    black_tile.min_uv = glm::vec2(0.83333f, 0.0f);
    black_tile.max_uv = glm::vec2(1.0f, 0.11111f);
    black_tile.rad = glm::vec2(0.5f);

    //rock
    rock.min_uv = glm::vec2(0.83333f, 0.11111f);
    rock.max_uv = glm::vec2(1.0f, 0.22222f);
    rock.rad = glm::vec2(0.5f);

    //money bag
    money_bag.min_uv = glm::vec2(0.83333f, 0.22222f);
    money_bag.max_uv = glm::vec2(1.0f, 0.33333f);
    money_bag.rad = glm::vec2(0.5f);

    //man
    man.min_uv = glm::vec2(0.83333f, 0.33333f);
    man.max_uv = glm::vec2(1.0f, 0.44444f);
    man.rad = glm::vec2(0.5f);

    //instructions
    game_start.min_uv = glm::vec2(0.5f, 0.88888f);
    game_start.max_uv = glm::vec2(1.0f, 0.99999f);
    game_start.rad = glm::vec2(0.5f);

    game_end.min_uv = glm::vec2(0.0f, 0.88888f);
    game_end.max_uv = glm::vec2(0.5f, 0.99999f);
    game_end.rad = glm::vec2(0.5f);

    mine_with_space.min_uv = glm::vec2(0.0f, 0.99999f);
    mine_with_space.max_uv = glm::vec2(0.5f, 1.0f);
    mine_with_space.rad = glm::vec2(0.5f);

    struct ValidDirections {
        bool left = false, right = false, up = false, down = false;
    };
    
    ValidDirections tile_dir[TILE_X][TILE_Y];

    //tiles where going left is valid
    tile_dir[1][0].left = tile_dir[2][0].left
        = tile_dir[3][0].left
        = tile_dir[3][1].left
        = tile_dir[1][2].left
        = tile_dir[2][2].left
        = tile_dir[4][2].left
        = tile_dir[2][3].left
        = tile_dir[3][3].left
        = tile_dir[4][3].left
        = tile_dir[1][5].left
        = tile_dir[2][5].left
        = tile_dir[3][5].left
        = tile_dir[4][5].left
        = true;

    //tiles where going right is valid
    tile_dir[0][0].right = tile_dir[0][1].right
        = tile_dir[2][0].right
        = tile_dir[2][1].right
        = tile_dir[0][2].right
        = tile_dir[1][2].right
        = tile_dir[3][2].right
        = tile_dir[1][4].right
        = tile_dir[2][4].right
        = tile_dir[3][4].right
        = tile_dir[0][5].right
        = tile_dir[1][5].right
        = tile_dir[2][5].right
        = tile_dir[3][5].right
        = true;

    //tiles where going up is valid
    tile_dir[1][1].up = tile_dir[2][1].up
        = tile_dir[3][1].up
        = tile_dir[4][1].up
        = tile_dir[0][2].up
        = tile_dir[2][2].up
        = tile_dir[3][2].up
        = tile_dir[4][2].up
        = tile_dir[2][3].up
        = tile_dir[1][4].up
        = tile_dir[3][4].up
        = tile_dir[4][4].up
        = tile_dir[0][5].up
        = tile_dir[1][5].up
        = tile_dir[2][5].up
        = tile_dir[3][5].up
        = true;

    //tiles where going down is valid
    tile_dir[1][0].down = tile_dir[2][0].down
        = tile_dir[3][0].down
        = tile_dir[4][0].down
        = tile_dir[0][1].down
        = tile_dir[2][1].down
        = tile_dir[3][1].down
        = tile_dir[4][1].down
        = tile_dir[2][2].down
        = tile_dir[1][3].down
        = tile_dir[3][3].down
        = tile_dir[4][3].down
        = tile_dir[0][4].down
        = tile_dir[1][4].down
        = tile_dir[2][4].down
        = tile_dir[3][4].down
        = true;

	//------------ game state ------------

	//glm::vec2 mouse = glm::vec2(0.0f, 0.0f); //mouse position in [-1,1]x[-1,1] coordinates

    //determine location of money bag
    int money_tile = rand() % ROCKS;
    bool found = false;

    //location of rocks, whether rock mined
    std::pair<int, int> rock_loc[ROCKS] = {
        std::make_pair(4, 0),
        std::make_pair(0, 1),
        std::make_pair(0, 2),
        std::make_pair(2, 4),
        std::make_pair(4, 5) };
    int rock_mined[ROCKS] = { 0 };

    float money_u = (rock_loc[money_tile].first + 0.5f) / 5.0f;
    float money_v = (rock_loc[money_tile].second + 0.5f) / 7.0f;

    //set player position at starting tile
    std::pair<int, int> man_xy = std::make_pair(2, 2);

    // set display text
    int display_text = 0;

    // initialise map grid, unexplored tiles are 0
    int map_grid[TILE_X][TILE_Y] = { 0 };
    map_grid[2][2] = 1;  // player starting tile

	struct {
		glm::vec2 at = glm::vec2(0.0f, 0.0f);
		glm::vec2 radius = glm::vec2(10.0f, 10.0f);
	} camera;
	//correct radius for aspect ratio:
	camera.radius.x = camera.radius.y * (float(config.size.x) / float(config.size.y));

	//------------ game loop ------------

	bool should_quit = false;

	while (true) {
		static SDL_Event evt;
        while (SDL_PollEvent(&evt) == 1) {

            //handle input:
            if (evt.type == SDL_KEYDOWN) {

                switch (evt.key.keysym.sym) {
                case SDLK_LEFT:
                    if (tile_dir[man_xy.first][man_xy.second].left) {
                        man_xy.first -= 1;
                    }
                    break;
                case SDLK_RIGHT:
                    if (tile_dir[man_xy.first][man_xy.second].right) {
                        man_xy.first += 1;
                    }
                    break;
                case SDLK_UP:
                    if (tile_dir[man_xy.first][man_xy.second].up) {
                        man_xy.second -= 1;
                    }
                    break;
                case SDLK_DOWN:
                    if (tile_dir[man_xy.first][man_xy.second].down) {
                        man_xy.second += 1;
                    }
                    break;
                case SDLK_SPACE:
                    for (int i = 0; i < ROCKS; i++) {
                        if (man_xy == rock_loc[i]) {
                            rock_mined[i] = true;
                            found = i == money_tile;
                        }
                    }
                    break;
                case SDLK_ESCAPE:
                    should_quit = true;
                    break;
                }

            }
            else if (evt.type == SDL_QUIT) {
                should_quit = true;
                break;
            }
        }

        if (should_quit) break;

        /*
		auto current_time = std::chrono::high_resolution_clock::now();
		static auto previous_time = current_time;
		float elapsed = std::chrono::duration< float >(current_time - previous_time).count();
		previous_time = current_time;
        */

        { //update game state:
            map_grid[man_xy.first][man_xy.second] = 1;

            display_text = 0

                for (int i = 0; i < ROCKS; i++) {
                    if (man_xy == rock_loc[i] && !rock_mined[i]) {
                        display_text = 1;
                    }
                }

            if (found) {
                display_text = 2;
            }
        }

		//draw output:
		glClearColor(0.5, 0.5, 0.5, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


		{ //draw game state:
			std::vector< Vertex > verts;

			//helper: add rectangle to verts:
			auto rect = [&verts](glm::vec2 const &at, glm::vec2 const &rad, glm::u8vec4 const &tint) {
				verts.emplace_back(at + glm::vec2(-rad.x,-rad.y), glm::vec2(0.0f, 0.0f), tint);
				verts.emplace_back(verts.back());
				verts.emplace_back(at + glm::vec2(-rad.x, rad.y), glm::vec2(0.0f, 1.0f), tint);
				verts.emplace_back(at + glm::vec2( rad.x,-rad.y), glm::vec2(1.0f, 0.0f), tint);
				verts.emplace_back(at + glm::vec2( rad.x, rad.y), glm::vec2(1.0f, 1.0f), tint);
				verts.emplace_back(verts.back());
			};

			auto draw_sprite = [&verts](SpriteInfo const &sprite, glm::vec2 const &at, float angle = 0.0f) {
				glm::vec2 min_uv = sprite.min_uv;
				glm::vec2 max_uv = sprite.max_uv;
				glm::vec2 rad = sprite.rad;
				glm::u8vec4 tint = glm::u8vec4(0xff, 0xff, 0xff, 0xff);
				glm::vec2 right = glm::vec2(std::cos(angle), std::sin(angle));
				glm::vec2 up = glm::vec2(-right.y, right.x);

				verts.emplace_back(at + right * -rad.x + up * -rad.y, glm::vec2(min_uv.x, min_uv.y), tint);
				verts.emplace_back(verts.back());
				verts.emplace_back(at + right * -rad.x + up * rad.y, glm::vec2(min_uv.x, max_uv.y), tint);
				verts.emplace_back(at + right *  rad.x + up * -rad.y, glm::vec2(max_uv.x, min_uv.y), tint);
				verts.emplace_back(at + right *  rad.x + up *  rad.y, glm::vec2(max_uv.x, max_uv.y), tint);
				verts.emplace_back(verts.back());
			};

            // always draw entire map
            draw_sprite(map, glm::vec2(0.0f, 0.0f));
            //  draw sprite player
            float player_u = (man_xy.first + 0.5f) / 5.0f;
            float player_v = (man_xy.second + 0.5f) / 7.0f;
            draw_sprite(man, glm::vec2(player_u, player_v));
            // always draw money bag           
            draw_sprite(money_bag, glm::vec2(money_u, money_v));

            // set map to be unveiled at player position, tile not covered if 1
            for (int i = 0; i < TILE_X; i++) {
                for (int j = 0; j < TILE_Y; j++) {
                    if (!map_grid[i][j]) {
                        int coord_u = (i + 0.5f) / 5.0f;
                        int coord_v = (j + 0.5f) / 7.0f;
                        draw_sprite(black_tile, glm::vec2(coord_u, coord_v));
                    }
                }
            }

            // rock drawned if not mined and unveiled
            for (int i = 0; i < ROCKS; i++) {
                if (!rock_mined[i]
                    && map_grid[rock_loc[i].first][rock_loc[i].second]) {
                    int coord_u = (rock_loc[i].first + 0.5f) / 5.0f;
                    int coord_v = (rock_loc[i].second + 0.5f) / 7.0f;
                    draw_sprite(rock, glm::vec2(coord_u, coord_v));
                }
            }

            // draw display text
            float display_text_x = 0.5f;
            float display_text_y = 6.5f / 7.0f;
            glm::vec2 display_text_loc = glm::vec2(display_text_x, display_text_y);
            switch (display_text) {
            case 0:
                draw_sprite(game_start, display_text_loc);
                break;
            case 1:
                draw_sprite(mine_with_space, display_text_loc);
                break;
            default:
                draw_sprite(game_end, display_text_loc);
                break;
        }


			rect(glm::vec2(0.0f, 0.0f), glm::vec2(4.0f), glm::u8vec4(0xff, 0x00, 0x00, 0xff));
			rect(mouse * camera.radius + camera.at, glm::vec2(4.0f), glm::u8vec4(0xff, 0xff, 0xff, 0x88));


			glBindBuffer(GL_ARRAY_BUFFER, buffer);
			glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * verts.size(), &verts[0], GL_STREAM_DRAW);

			glUseProgram(program);
			glUniform1i(program_tex, 0);
			glm::vec2 scale = 1.0f / camera.radius;
			glm::vec2 offset = scale * -camera.at;
			glm::mat4 mvp = glm::mat4(
				glm::vec4(scale.x, 0.0f, 0.0f, 0.0f),
				glm::vec4(0.0f, scale.y, 0.0f, 0.0f),
				glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
				glm::vec4(offset.x, offset.y, 0.0f, 1.0f)
			);
			glUniformMatrix4fv(program_mvp, 1, GL_FALSE, glm::value_ptr(mvp));

			glBindTexture(GL_TEXTURE_2D, tex);
			glBindVertexArray(vao);

			glDrawArrays(GL_TRIANGLE_STRIP, 0, verts.size());
		}


		SDL_GL_SwapWindow(window);
	}


	//------------  teardown ------------

	SDL_GL_DeleteContext(context);
	context = 0;

	SDL_DestroyWindow(window);
	window = NULL;

	return 0;
}



static GLuint compile_shader(GLenum type, std::string const &source) {
	GLuint shader = glCreateShader(type);
	GLchar const *str = source.c_str();
	GLint length = source.size();
	glShaderSource(shader, 1, &str, &length);
	glCompileShader(shader);
	GLint compile_status = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
	if (compile_status != GL_TRUE) {
		std::cerr << "Failed to compile shader." << std::endl;
		GLint info_log_length = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_length);
		std::vector< GLchar > info_log(info_log_length, 0);
		GLsizei length = 0;
		glGetShaderInfoLog(shader, info_log.size(), &length, &info_log[0]);
		std::cerr << "Info log: " << std::string(info_log.begin(), info_log.begin() + length);
		glDeleteShader(shader);
		throw std::runtime_error("Failed to compile shader.");
	}
	return shader;
}

static GLuint link_program(GLuint fragment_shader, GLuint vertex_shader) {
	GLuint program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);
	GLint link_status = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &link_status);
	if (link_status != GL_TRUE) {
		std::cerr << "Failed to link shader program." << std::endl;
		GLint info_log_length = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length);
		std::vector< GLchar > info_log(info_log_length, 0);
		GLsizei length = 0;
		glGetProgramInfoLog(program, info_log.size(), &length, &info_log[0]);
		std::cerr << "Info log: " << std::string(info_log.begin(), info_log.begin() + length);
		throw std::runtime_error("Failed to link program");
	}
	return program;
}
