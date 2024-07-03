/*
** RPI Game Architecture Engine
**
** Portions adapted from:
** Viper Engine - Copyright (C) 2016 Velan Studios - All Rights Reserved
**
** This file is distributed under the MIT License. See LICENSE.txt.
*/

#include "ga_mat2f.h"
#include "ga_math.h"

void ga_mat2f::make_identity()
{
	for (int i = 0; i < 2; ++i)
	{
		for (int j = 0; j < 2; ++j)
		{
			data[i][j] = i == j ? 1.0f : 0.0f;
		}
	}
}

ga_mat2f ga_mat2f::operator*(const ga_mat2f& __restrict b) const
{
	ga_mat2f result;
	for (int i = 0; i < 2; ++i)
	{
		for (int j = 0; j < 2; ++j)
		{
			float tmp = 0.0f;
			for (int k = 0; k < 2; ++k)
			{
				tmp += data[i][k] * b.data[k][j];
			}
			result.data[i][j] = tmp;
		}
	}
	return result;
}

ga_mat2f& ga_mat2f::operator*=(const ga_mat2f& __restrict m)
{
	(*this) = (*this) * m;
	return (*this);
}

void ga_mat2f::transpose()
{
	ga_mat2f tmp;
	for (int i = 0; i < 2; ++i)
	{
		for (int j = 0; j < 2; ++j)
		{
			tmp.data[i][j] = data[j][i];
		}
	}
	(*this) = tmp;
}

void ga_mat2f::invert()
{
	ga_mat2f tmp = *this;
    double det = data[0][0] * data[1][1] - data[0][1] * data[1][0];

    //VLOG_ASSERT(inv_det != 0.0f, k_vlog_error, 100, "mat2f", "Attempting to invert matrix with zero determinant.");

    double invDet = 1.0 / det;

    // Calculate the inverse using the formula for a 2x2 matrix
    data[0][0] =  tmp.data[1][1] * invDet;
    data[0][1] = -tmp.data[0][1] * invDet;
    data[1][0] = -tmp.data[1][0] * invDet;
    data[1][1] =  tmp.data[0][0] * invDet;
}

bool ga_mat2f::equal(const ga_mat2f& __restrict b)
{
	bool is_not_equal = false;
	for (int i = 0; i < 2; ++i)
	{
		for (int j = 0; j < 2; ++j)
		{
			is_not_equal = is_not_equal || !ga_equalf(data[i][j], b.data[i][j]);
		}
	}
	return !is_not_equal;
}
