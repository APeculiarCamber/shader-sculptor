/*
** RPI Game Architecture Engine
**
** Portions adapted from:
** Viper Engine - Copyright (C) 2016 Velan Studios - All Rights Reserved
**
** This file is distributed under the MIT License. See LICENSE.txt.
*/

#include "ga_material.h"
#include "ss_graph.hpp"

#include <iostream>
#include <string>
#include <chrono>

using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::system_clock;

ga_material::ga_material()
{
}

ga_material::~ga_material()
{
	if (_vs) { delete _vs; }
	if (_fs) { delete _fs; }
	if (_program) { delete _program; }
}

bool ga_material::init(std::string& source_vs, std::string& source_fs)
{
	_vs = new ga_shader(source_vs.c_str(), GL_VERTEX_SHADER);
	if (not _vs->compile()) {
        assert("Failed to compile vertex shader");
	}

	_fs = new ga_shader(source_fs.c_str(), GL_FRAGMENT_SHADER);
	if (not _fs->compile()) {
        assert("Failed to compile fragment shader");
	}

	_program = new ga_program();
	_program->attach(*_vs);
	_program->attach(*_fs);
	if (not _program->link()) {
        assert("Failed to link shader program");
	}

	return true;
}

unsigned int ga_material::set_uniforms_by_type(Parameter_Data* p_data, unsigned int texture_id) {
	if (p_data->GetParamType() == SS_Texture2D) {
		_program->get_uniform(p_data->GetName()).set((const unsigned int*)p_data->GetData(), texture_id);
		return texture_id + 1;
	}
	if (p_data->GetParamType() == SS_Float) {
		switch (p_data->GetParamGenType()) {
			case SS_Mat2: _program->get_uniform(p_data->GetName()).set(*(const ga_mat2f*)p_data->GetData()); break;
			case SS_Mat3: _program->get_uniform(p_data->GetName()).set(*(const ga_mat3f*)p_data->GetData()); break;
			case SS_Mat4: _program->get_uniform(p_data->GetName()).set(*(const ga_mat4f*)p_data->GetData()); break;
			case SS_Scalar: _program->get_uniform(p_data->GetName()).set(*(const float*)p_data->GetData()); break;
			case SS_Vec2: _program->get_uniform(p_data->GetName()).set(*(const ga_vec2f*)p_data->GetData()); break;
			case SS_Vec3: _program->get_uniform(p_data->GetName()).set(*(const ga_vec3f*)p_data->GetData()); break;
			case SS_Vec4: _program->get_uniform(p_data->GetName()).set(*(const ga_vec4f*)p_data->GetData()); break;
            case SS_MAT:
                break;
        }
	}
	return texture_id;
}

void ga_material::bind(const ga_mat4f& view, const ga_mat4f& proj, const ga_mat4f& transform, const std::vector<std::unique_ptr<Parameter_Data>>& p_datas)
{
	_program->use();
	ga_mat4f view_proj = view * proj;

	unsigned int current_tex_id = 0;
	for (const auto& p_data : p_datas) {
		current_tex_id = set_uniforms_by_type(p_data.get(), current_tex_id);
	}
	
	// ga_uniform texture_uniform = _program->get_uniform("u_texture");
	// texture_uniform.set(*_texture, 0);

	ga_uniform mvp_uniform = _program->get_uniform("u_mvp");
	mvp_uniform.set(transform * view_proj);
	_program->get_uniform("u_model_mat").set(transform);
	_program->get_uniform("u_objectPos").set(transform.get_translation());
	_program->get_uniform("u_base_color").set(transform.get_translation());

        auto end = std::chrono::system_clock::now();
		float f = std::chrono::duration_cast<std::chrono::milliseconds>(end - k_ga_material_clock_start).count();

	float seconds = f / 1000.0f;

	_program->get_uniform("u_time").set(seconds);
	
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
}


void ga_pbr_material::bind(const ga_mat4f& view, const ga_mat4f& proj, const ga_mat4f& transform, const std::vector<std::unique_ptr<Parameter_Data>>& p_datas) {
	_program->use();
	ga_mat4f view_proj = view * proj;

	unsigned int current_tex_id = 0;
	for (const auto& p_data : p_datas) {
		current_tex_id = set_uniforms_by_type(p_data.get(), current_tex_id);
	}
	
	// ga_uniform texture_uniform = _program->get_uniform("u_texture");
	// texture_uniform.set(*_texture, 0);

	ga_uniform mvp_uniform = _program->get_uniform("u_mvp");
	mvp_uniform.set(transform * view_proj);
	_program->get_uniform("u_model_mat").set(transform);
	_program->get_uniform("u_objectPos").set(transform.get_translation());
	_program->get_uniform("u_base_color").set(transform.get_translation());
	
	_program->get_uniform("u_eyePos").set(view.inverse().get_translation());

    auto end = std::chrono::system_clock::now();
    float f = (float)std::chrono::duration_cast<std::chrono::milliseconds>(end - k_ga_material_clock_start).count();

	float seconds = f / 1000.0f;

	_program->get_uniform("u_time").set(seconds);
	float s1 = 1.0f;
	float s2 = 1.3f;

	ga_vec3f lp1 = {2*cos(seconds*s1), -2, 2*sin(seconds*s1)}, lc1 = {10, 10, 10};
	ga_vec3f lp2 = {2*sin(seconds*s2), 2, 2*cos(seconds*s2)}, lc2 = {10, 10, 10};
	_program->get_uniform("u_lightPositions[0]").set(lp1);
	_program->get_uniform("u_lightColors[0]").set(lc1);
	_program->get_uniform("u_lightPositions[1]").set(lp2);
	_program->get_uniform("u_lightColors[1]").set(lc2);

	ga_vec3f ze = {0, 0, 0};
	_program->get_uniform("u_lightColors[2]").set(ze);
	_program->get_uniform("u_lightColors[3]").set(ze);
	
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
}