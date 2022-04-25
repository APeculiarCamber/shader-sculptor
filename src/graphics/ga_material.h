#pragma once

/*
** RPI Game Architecture Engine
**
** Portions adapted from:
** Viper Engine - Copyright (C) 2016 Velan Studios - All Rights Reserved
**
** This file is distributed under the MIT License. See LICENSE.txt.
*/

#include "ga_program.h"
#include "ga_texture.h"

#include "../math/ga_mat4f.h"
#include "../math/ga_vec3f.h"

#include <string>
#include <vector>
#include <chrono>

/*
** Base class for all graphical materials.
** Includes the shaders and other state necessary to draw geometry.
*/
static std::chrono::system_clock::time_point k_ga_material_clock_start = std::chrono::system_clock::now();

class ga_material
{
public:
	ga_material();
	~ga_material();

	virtual bool init(std::string& source_vs, std::string& source_fs);
	virtual unsigned int set_uniforms_by_type(struct Parameter_Data* p_data, unsigned int texture_id);

	virtual void bind(const ga_mat4f& view, const ga_mat4f& proj, const ga_mat4f& transform, std::vector<struct Parameter_Data*>& p_data);

	virtual void update_vertex_shader(std::string& vert_shader_code);
	virtual void update_frag_shader(std::string& frag_shader_code);
	ga_shader* _vs;
	ga_shader* _fs;
	ga_program* _program;
	

};


/*
** Base class for all graphical materials.
** Includes the shaders and other state necessary to draw geometry.
*/
class ga_pbr_material : ga_material
{
public:
	void bind(const ga_mat4f& view, const ga_mat4f& proj, const ga_mat4f& transform, std::vector<struct Parameter_Data*>& p_data) override;

	ga_vec3f albedo;
	float metalness;
	float roughness;
	float ao;
};
