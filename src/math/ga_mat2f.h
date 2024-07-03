#pragma once

/*
** RPI Game Architecture Engine
**
** Portions adapted from:
** Viper Engine - Copyright (C) 2016 Velan Studios - All Rights Reserved
**
** This file is distributed under the MIT License. See LICENSE.txt.
*/

#include "ga_vec2f.h"
#include "ga_vec3f.h"

/*
** Floating point 2x2 matrix.
*/
struct ga_mat2f
{
	float data[2][2];

	/*
	** Build an identity matrix.
	*/
	void make_identity();

	/*
	** Multiply two matrices and store the result in a third.
	*/
    ga_mat2f operator*(const ga_mat2f& __restrict b) const;

	/*
	** Multiply a matrix by another, storing the result in the first.
	*/
    ga_mat2f& operator*=(const ga_mat2f& __restrict m);

	/*
	** Transform a vector by a matrix.
	*/
	ga_vec2f transform(const ga_vec2f& __restrict in) const;

	/*
	** Transpose a matrix.
	*/
	void transpose();

	/*
	** Invert the given matrix.
	*/
	void invert();

	/*
	** Determine if two matrices are largely equivalent.
	*/
	bool equal(const ga_mat2f& __restrict b);
};
