
#include "renderer.h"
#include "lib/renderer_support.lua.h"

namespace aroma {

	GLuint make_float_buffer(float* parts, size_t size) {
		GLuint buffer;
		glGenBuffers(1, &buffer);

		glBindBuffer(GL_ARRAY_BUFFER, buffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * size, parts, GL_STATIC_DRAW);
		return buffer;
	}

	void Renderer::img_rect(const Image* img, float x, float y, float r, float sx,
			float sy, float ox, float oy)
	{
		float tex_coords[] = {
			0,0,
			1,0,
			0,1,
			1,1
		};

		float x2 = x + img->width;
		float y2 = y + img->height;

		float verts[] = {
			x,y,
			x2,y,
			x,y2,
			x2,y2
		};

		GLuint vert_buffer = make_float_buffer(verts, 8);
		GLuint tex_buffer = make_float_buffer(tex_coords, 8);

		default_shader->set_uniform("texturing", 1u);
		img->bind();
		default_shader->set_uniform("tex", 0u);

		GLuint P = default_shader->attr_loc("P");
		GLuint T = default_shader->attr_loc("T");

		glEnableVertexAttribArray(P);
		glBindBuffer(GL_ARRAY_BUFFER, vert_buffer);
		glVertexAttribPointer(P, 2, GL_FLOAT, false, 0, 0);

		glEnableVertexAttribArray(T);
		glBindBuffer(GL_ARRAY_BUFFER, tex_buffer);
		glVertexAttribPointer(T, 2, GL_FLOAT, false, 0, 0);

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		glBindBuffer(GL_ARRAY_BUFFER, 0);

		glDeleteBuffers(1, &vert_buffer);
		glDeleteBuffers(1, &tex_buffer);
	}

	void Renderer::rect(float x1, float y1, float x2, float y2) {
		float verts[] = {
			x1,y1,
			x2,y1,
			x1,y2,
			x2,y2
		};

		GLuint vert_buffer = make_float_buffer(verts, 8);

		default_shader->set_uniform("texturing", 0u);

		GLuint P = default_shader->attr_loc("P");

		glDisableVertexAttribArray(default_shader->attr_loc("T"));

		glEnableVertexAttribArray(P);
		glBindBuffer(GL_ARRAY_BUFFER, vert_buffer);
		glVertexAttribPointer(P, 2, GL_FLOAT, false, 0, 0);

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		glBindBuffer(GL_ARRAY_BUFFER, 0);

		glDeleteBuffers(1, &vert_buffer);
	}

	Renderer::Renderer(GLContext* context, LuaBinding* binding) :
		context(context),
		binding(binding),
		default_shader(NULL),
		_texturing(false)
	{
		context->set_renderer(this);
		binding->bind_module(this);
	}

	bool Renderer::init() {
		log("init renderer\n");
		glClearColor(0.1, 0.1, 0.1, 1.0);

		if (!binding->load_and_run(
					renderer_support_lua,
					renderer_support_lua_len,
					"renderer_support.lua"))
		{
			return false;
		}

		if (!default_shader) {
			err("you forgot to create a default shader!\n");
			return false;
		}

		last_time = context->get_time();
		return true;
	}

	void Renderer::draw(double dt) {
		glClear(GL_COLOR_BUFFER_BIT);

		if (!default_shader) return;

		// load uniforms
		default_shader->bind();
		default_shader->set_uniform("C", current_color);
		default_shader->set_uniform("PMatrix",
				Mat4::ortho2d(0, context->width(), 0, context->height()));

		lua_State* l = binding->lua();

		int i = lua_gettop(l);
		binding->push_self();

		lua_getfield(l, -1, "update");
		if (!lua_isnil(l, -1)) {
			lua_pushnumber(l, dt);
			lua_call(l, 1, 0);
		} else {
			lua_pop(l, 1); // pop nil
		}

		lua_getfield(l, -1, "draw");
		if (!lua_isnil(l, -1)) {
			lua_call(l, 0, 0);
		} else {
			lua_pop(l, 1); // pop nil
		}

		lua_settop(l, i);
	}

	// called for every frame
	void Renderer::tick() {
		context->make_current();

		glViewport(0, 0, context->width(), context->height());

		double time = context->get_time();
		draw(time - last_time);
		last_time = time;

		context->flush();
	}

	void Renderer::reshape(const int w, const int h) {
		context->resize(w, h);
		if (!context->is_flushing()) {
			tick(); // start it up
		}
	}

	const char* Renderer::module_name() {
		return "graphics";
	}

	void Renderer::texturing(bool enabled) {
		_texturing = enabled;
	}

	// write all the funcs into the current table
	void Renderer::bind_all(lua_State *l) {
		set_new_func("setColor", _setColor);
		set_new_func("rectangle", _rectangle);
		set_new_func("draw", _draw);
		set_new_func("setDefaultShader", _setDefaultShader);

		set_new_func("newShader", Shader::_new);
	}

	int Renderer::_setColor(lua_State *l) {
		Renderer *self = upvalue_self(Renderer);
		self->current_color = Color::pop(l);
		return 0;
	}

	int Renderer::_rectangle(lua_State *l) {
		Renderer *self = upvalue_self(Renderer);
		Rect r = Rect::pop(l);
		self->rect(r.x, r.y, r.x + r.w, r.y + r.h);
		return 0;
	}

	// thing, x, y, r, sx, sy, ox, oy
	int Renderer::_draw(lua_State *l) {
		Renderer *self = upvalue_self(Renderer);
		if (self->binding->is_type(1, "Image")) {
			Image* img = getself(Image);
			float x = lua_tonumber(l, 2);
			float y = lua_tonumber(l, 3);
			self->img_rect(img, x, y);
		} else {
			return luaL_error(l, "unknown value passed to draw");
		}
		// right now argument 1 can only be an image
		return 0;
	}

	int Renderer::_setDefaultShader(lua_State *l) {
		Renderer* self = upvalue_self(Renderer);
		Shader* shader = getself(Shader);
		if (shader->link()) {
			self->default_shader = shader;
		}
		return 0;
	}

}
