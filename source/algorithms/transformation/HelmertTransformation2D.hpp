// Description: 2D Helmert transformation

// Copyright (c) 2010 - 2013
// Tomas Bayer
// Charles University in Prague, Faculty of Science
// bayertom@natur.cuni.cz

// This library is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this library. If not, see <http://www.gnu.org/licenses/>.


#ifndef HelmertTransformation2D_HPP
#define HelmertTransformation2D_HPP

#include <algorithm>
#include <cmath>

#include "libalgo/source/structures/point/Point3DCartesian.h"

#include "libalgo/source/exceptions/ErrorBadData.h"
#include "libalgo/source/exceptions/ErrorMathZeroDevision.h"


template <typename Point1, typename Point2, typename Point3, TDestructable destructable, TDestructable destructable2, TDestructable destructable3>
void HelmertTransformation2D::transformPoints ( const Container <Point1 *, destructable> &global_points, const Container <Point2 *, destructable2> &local_points, Container <Point3 *, destructable3> &transformed_points,
                TTransformationKeyHelmert2D <typename Point1::Type> &key_helmert, const bool print_exception, std::ostream * output )
{
        //Compute non weighted 2D Helmert transformation
        typename TWeights <typename Point1::Type> ::Type weights ( global_points.size(), 1.0 );
        transformPoints ( global_points, local_points, transformed_points, weights, key_helmert, print_exception, output );
}


template <typename Point1, typename Point2, typename Point3, TDestructable destructable, TDestructable destructable2, TDestructable destructable3>
void HelmertTransformation2D::transformPoints ( const Container <Point1 *, destructable> &global_points, const Container <Point2 *, destructable2> &local_points, Container <Point3 *, destructable3> &transformed_points,
                typename TWeights <typename Point1::Type> ::Type & weights, TTransformationKeyHelmert2D <typename Point1::Type> &key_helmert, const bool print_exception, std::ostream * output )
{
        //Compute 2D Helmert weighted transformation: overloaded method
        getTransformKey ( global_points, local_points, weights, key_helmert );
        transform ( global_points, local_points, transformed_points, key_helmert );
}


template <typename Point1, typename Point2, TDestructable destructable, TDestructable destructable2>
void HelmertTransformation2D::getTransformKey ( const Container <Point1 *, destructable> &global_points, const Container <Point2 *, destructable2> &local_points, TTransformationKeyHelmert2D <typename Point1::Type> &key_helmert )
{
        //Get transformation key: non weighted 2D Helmert transformation
        typename TWeights <typename Point1::Type> ::Type weights ( global_points.size(), 1.0 );
        getTransformKey ( global_points, local_points, weights, key_helmert );
}


template <typename Point1, typename Point2, TDestructable destructable, TDestructable destructable2>
void HelmertTransformation2D::getTransformKey ( const Container <Point1 *, destructable> &global_points, const Container <Point2 *, destructable2> &local_points, typename TWeights <typename Point1::Type> ::Type & weights,
                TTransformationKeyHelmert2D <typename Point1::Type> &key_helmert )
{
        //Get transformation key for weighted transformation
        const unsigned int n_global = global_points.size(), n_local = local_points.size();
        typename Point1::Type sumx_local = 0, sumy_local = 0, sumx_global = 0, sumy_global = 0;

        //Not enough points
        if ( ( n_global < 2 ) || ( n_local < 2 ) )
        {
                throw  ErrorBadData ( "ErrorBadData: not enough points. ", "Can not compute Helmert 2D transformation key. \n" );
        }

        //Less local points
        if ( n_global > n_local )
        {
                throw  ErrorBadData ( "ErrorBadData: less local points than global points. ", "Can not compute Helmert 2D transformation key. \n" );
        }

        //Compute sums of coordinates, weights
        typename Point1::Type sum_weights = 0;

        for ( unsigned int i = 0; i < n_global; i++ )
        {
                sumx_local += weights[i] * local_points [i]->getX();
                sumy_local += weights[i] * local_points [i]->getY();
                sumx_global += weights[i] * global_points [i]->getX();
                sumy_global += weights[i] * global_points [i]->getY();

                sum_weights += weights[i];
        }

        //Compute center of mass
        key_helmert.x_mass_local = sumx_local / ( sum_weights );
        key_helmert.y_mass_local = sumy_local / ( sum_weights );
        key_helmert.x_mass_global = sumx_global / ( sum_weights );
        key_helmert.y_mass_global = sumy_global / ( sum_weights );

        //Remeber k
        key_helmert.k = sum_weights;

        //Reduction of coordinates to the center of mass
        typename Point1::Type x_red_local, y_red_local, xred_global, yred_global, k1 = 0, k2 = 0;

        //Process all points
        key_helmert.J = 0;

        for ( unsigned int i = 0; i < n_global; i++ )
        {
                //Compute reduced coordinates
                x_red_local = local_points [i]->getX() - key_helmert.x_mass_local;
                y_red_local = local_points [i]->getY() - key_helmert.y_mass_local;
                xred_global = global_points [i]->getX() - key_helmert.x_mass_global;
                yred_global = global_points [i]->getY() - key_helmert.y_mass_global;

                //Compute coefficients of transformation
                key_helmert.J += weights[i] * ( x_red_local * x_red_local + y_red_local * y_red_local );
                k1 += weights[i] * ( xred_global * x_red_local + yred_global * y_red_local );
                k2 += weights[i] * ( yred_global * x_red_local - xred_global * y_red_local );
        }

        //Throw exception
        if ( key_helmert.J == 0 )
        {
                throw  ErrorMathZeroDevision <typename Point1::Type> ( "ErrorMathZeroDevision: can not compute Helmert 2D transformation key, ", " divider = 0.", key_helmert.J );
        }

        //Transformation coefficients
        key_helmert.c1 = k1 / key_helmert.J;
        key_helmert.c2 = k2 / key_helmert.J;
}


template <typename Point1, typename Point2, typename Point3, TDestructable destructable, TDestructable destructable2, TDestructable destructable3>
void HelmertTransformation2D::transform ( const Container <Point1 *, destructable> &global_points, const Container <Point2 *, destructable2> &local_points, Container <Point3 *, destructable3> &transformed_points, const TTransformationKeyHelmert2D <typename Point1::Type> &key_helmert )

{
        //Transform all points using 2D Helmert transformation ( weighted / non weighted )
        if ( transformed_points.size() != 0 )
        {
                throw ErrorBadData ( "ErrorBadData: list of tranformed points is not empty. ", "Can not compute Helmert 2D transformation." );
        }

        const unsigned int n = local_points.size();

        for ( unsigned int i = 0; i < n; i++ )
        {
                //Reduce coordinates
                const typename Point1::Type x_red_local = local_points [i]->getX() - key_helmert.x_mass_local,
                                            y_red_local = local_points [i]->getY() - key_helmert.y_mass_local;

                //Transform point, add coordinates center of mass
                const typename Point1::Type x_transform = key_helmert.c1 * x_red_local - key_helmert.c2 * y_red_local + key_helmert.x_mass_global,
                                            y_transform = key_helmert.c2 * x_red_local + key_helmert.c1 * y_red_local + key_helmert.y_mass_global;

                //Add point to the list
                transformed_points.push_back ( new Point3 ( x_transform, y_transform ) );
        }
}


#endif
