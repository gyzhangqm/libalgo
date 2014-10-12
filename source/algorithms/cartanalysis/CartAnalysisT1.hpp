// Description: Performs cartometric analysis (i.e. detection of the cartographic projection)

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


#ifndef CartAnalysis_HPP
#define CartAnalysis_HPP

#include <algorithm>
#include <iomanip>
#include <cmath>


#include "libalgo/source/const/Const.h"

#include "libalgo/source/structures/point/Node3DCartesianProjected.h"
#include "libalgo/source/structures/projection/Sample.h"

#include "libalgo/source/algorithms/cartdistortion/CartDistortion.h"
#include "libalgo/source/algorithms/carttransformation/CartTransformation.h"
#include "libalgo/source/algorithms/geneticalgorithms/DifferentialEvolution.h"
#include "libalgo/source/algorithms/transformation/HomotheticTransformation2D.h"
#include "libalgo/source/algorithms/transformation/HelmertTransformation2D.h"
#include "libalgo/source/algorithms/angle3points/Angle3Points.h"
#include "libalgo/source/algorithms/facearea/FaceArea.h"

#include "libalgo/source/algorithms/turningfunction/TurningFunction.h"
#include "libalgo/source/algorithms/nndistance/NNDistance.h"
#include "libalgo/source/algorithms/voronoi2D/Voronoi2D.h"
#include "libalgo/source/algorithms/minimumleastsquares/FAnalyzeProjA.h"
#include "libalgo/source/algorithms/minimumleastsquares/FAnalyzeProjA2.h"
#include "libalgo/source/algorithms/minimumleastsquares/FAnalyzeProjV.h"
#include "libalgo/source/algorithms/minimumleastsquares/FAnalyzeProjV2.h"
#include "libalgo/source/algorithms/simplexmethod/FAnalyzeProjV2S.h"
#include "libalgo/source/algorithms/geneticalgorithms/FAnalyzeProjV2DE.h"
#include "libalgo/source/algorithms/minimumleastsquares/FAnalyzeProjC.h"
#include "libalgo/source/algorithms/minimumleastsquares/MinimumLeastSquares.h"
#include "libalgo/source/algorithms/simplexmethod/SimplexMethod.h"

#include "libalgo/source/comparators/sortPointsByID.h"
#include "libalgo/source/comparators/sortPointsByX.h"
#include "libalgo/source/comparators/sortPointsByLat.h"
#include "libalgo/source/comparators/sortPointsByLon.h"
#include "libalgo/source/comparators/sortProjectionPolePositionsByLat.h"
#include "libalgo/source/comparators/sortProjectionPolePositionsByCompCriterium.h"

#include "libalgo/source/comparators/removeUnequalMeridianParallelPointIndices.h"
#include "libalgo/source/comparators/removeProjectionPolePositions.h"
#include "libalgo/source/comparators/findMeridianParallelPointIndices.h"
#include "libalgo/source/comparators/getSecondElementInPair.h"

#include "libalgo/source/comparators/sortSamplesByCrossNearestNeighbourDistanceRatio.h"
#include "libalgo/source/comparators/sortSamplesByAverageNearestNeighbourDistanceRatio.h"
#include "libalgo/source/comparators/sortSamplesByHomotheticTransformationRatio.h"
#include "libalgo/source/comparators/sortSamplesByHelmertTransformationRatio.h"
#include "libalgo/source/comparators/sortSamplesByAngularDifferencesRatio.h"
#include "libalgo/source/comparators/sortSamplesByGNTurningFunctionRatio.h"
#include "libalgo/source/comparators/sortSamplesByNNNGraphRatio.h"
#include "libalgo/source/comparators/sortSamplesBySphereOfInfluenceGraphRatio.h"
#include "libalgo/source/comparators/sortSamplesByVoronoiCellAreaLengthRatio.h"
#include "libalgo/source/comparators/sortSamplesByVoronoiCellTurningFunctionRatio.h"
#include "libalgo/source/comparators/sortSamplesByVoronoiCellTARRatio.h"
#include "libalgo/source/comparators/sortSamplesByVoronoiCellInnerDistanceRatio.h"
#include "libalgo/source/comparators/sortSamplesByAllRatios.h"

#include "libalgo/source/exceptions/ErrorMath.h"

#include "libalgo/source/io/DXFExport.h"


template <typename T>
void CartAnalysis::computeAnalysisForAllSamplesGS ( Container <Sample <T> > &sl, Container <Projection <T> *> &pl, Container <Node3DCartesian <T> *> &nl_test, Container <Point3DGeographic <T> *> &pl_reference,
                typename TMeridiansList <T> ::Type meridians, typename TParallelsList <T> ::Type parallels, const Container <Face <T> *> &faces_test, TAnalysisParameters <T> & analysis_parameters,
                unsigned int & total_created_or_thrown_samples, std::ostream * output )
{
        //Find mimum using the global sampling of the function

        //Total computed analysis ( successful + thrown by the heuristic )
        total_created_or_thrown_samples = 0;

        //Total successfully computed analysis for one cartographic projection
        unsigned int total_created_and_analyzed_samples_projection = 0;

        //Create sample for analyzed projection from command line and set flag for this sample
        if ( analysis_parameters.analyzed_projections.size() > 0 )
        {
                //Analyze all projections specified in command line
                for ( typename TItemsList<Projection <T> *> ::Type ::iterator i_projections = analysis_parameters.analyzed_projections.begin(); i_projections != analysis_parameters.analyzed_projections.end(); ++i_projections )
                {
                        //Get analyzed projection
                        Projection <T> *analyzed_proj = *i_projections;

                        //List of points using new central meridian redefined in projection file
                        Container <Point3DGeographic <T> *> pl_reference_red;

                        //Reduce lon using a new central meridian redefined in projection file, if necessary
                        if ( ( *i_projections )->getLon0() != 0.0 ) redLon ( pl_reference, ( *i_projections )->getLon0(), pl_reference_red );

                        //Set pointer to processed file: reduced or non reduced
                        Container <Point3DGeographic <T> *> * p_pl_reference = ( ( *i_projections )->getLon0() == 0.0 ? &pl_reference : &pl_reference_red );

                        //Create temporary containers for non singular points
                        Container <Node3DCartesian <T> *> nl_test_non_sing;
                        Container <Point3DGeographic <T> *> pl_reference_non_sing;

                        typename TDevIndexPairs <T>::Type non_singular_pairs;
                        TIndexList non_singular_points;

                        //Initialize non singular indices
                        for ( unsigned int i = 0; i < p_pl_reference->size(); i++ ) non_singular_points.push_back ( i );

                        //Set pointer to processed file: with or wihout singular points
                        Container <Node3DCartesian <T> *> *p_nl_test = &nl_test;

                        //Remove singular points to prevent throwing a sample
                        bool singular_points_found = false;
                        removeSingularPoints ( *p_nl_test, *p_pl_reference, *i_projections, nl_test_non_sing, pl_reference_non_sing, non_singular_pairs );

                        //Some singular points have been found
                        if ( nl_test.size() != nl_test_non_sing.size() )
                        {
                                //Set pointers to files without singular points
                                p_nl_test = &nl_test_non_sing;	p_pl_reference = &pl_reference_non_sing;

                                //Set flag to true, used for a sample using non-singular sets
                                singular_points_found = true;

                                //Correct meridians and parallels
                                correctMeridiansAndParrallels <T> ( meridians, parallels, non_singular_pairs );

                                //Convert non singular pairs to index list: indices will be printed in output
                                non_singular_points.clear();
                                std::transform ( non_singular_pairs.begin(), non_singular_pairs.end(), std::back_inserter ( non_singular_points ), getSecondElementInPair() );
                        }

                        //Compute analysis
                        try
                        {
                                Sample <T> analyzed_sample;
                                ( void ) computeAnalysisForOneSample ( *p_nl_test, *p_pl_reference, meridians, parallels, faces_test, analyzed_proj, analysis_parameters, analyzed_sample, singular_points_found, total_created_and_analyzed_samples_projection, output );

                                //Add result to the list
                                if ( total_created_and_analyzed_samples_projection ) sl.push_back ( analyzed_sample );
                        }

                        //Throw exception
                        catch ( Error & error )
                        {
                                if ( analysis_parameters.print_exceptions )
                                        error.printException();
                        }

                        //Sample with analyzed projection has been successfully created (not thrown by the heuristic)
                        if ( total_created_and_analyzed_samples_projection > 0 ) sl [sl.size() - 1].setAnalyzedProjectionSample ( true );
                }

                if ( total_created_and_analyzed_samples_projection == 0 ) throw ErrorBadData ( "ErrorBadData: no analyzed projection has been used because of dissimilarity.", "Analysis has been stopped." );
        }

        //Process all cartographic projections from the list one by one
        for ( typename TItemsList <Projection <T> *> ::Type ::const_iterator i_projections = pl.begin(); i_projections != pl.end(); ++ i_projections )
        {
                //Get limits of the cartographic pole latitude and longitude: some projections are defined only in normal position
                total_created_and_analyzed_samples_projection = 0;

                //Print actual projection name to the log
                *output << ( *i_projections ) -> getProjectionName() << ": ";

                //List of points using new central meridian redefined in projection file
                Container <Point3DGeographic <T> *> pl_reference_red;

                //Reduce lon using a new central meridian redefined in projection file, if necessary
                if ( ( *i_projections )->getLon0() != 0.0 ) redLon ( pl_reference, ( *i_projections )->getLon0(), pl_reference_red );

                //Set pointer to processed sets: reduced or non reduced
                Container <Point3DGeographic <T> *> * p_pl_reference = ( ( *i_projections )->getLon0() == 0.0 ? &pl_reference : &pl_reference_red );

                //Create list of possible pole positions
                typename TItemsList <TProjectionPolePosition<T> >::Type proj_pole_positions_list;

                //Get both latp and lonp intervals
                TMinMax <T> latp_interval_heur = ( *i_projections )->getLatPInterval();
                TMinMax <T> lonp_interval_heur = ( *i_projections )->getLonPInterval();

                //Find intervals of latp, lonp
                if ( analysis_parameters.perform_heuristic )
                        findLatPLonPIntervals ( *p_pl_reference, *i_projections, latp_interval_heur, lonp_interval_heur );

                //Normal aspect
                if ( analysis_parameters.analyze_normal_aspect )
                        createOptimalLatPLonPPositions ( *p_pl_reference, *i_projections, latp_interval_heur, lonp_interval_heur, analysis_parameters, NormalAspect, proj_pole_positions_list, output );

                //Transverse aspect
                if ( analysis_parameters.analyze_transverse_aspect )
                        createOptimalLatPLonPPositions ( *p_pl_reference, *i_projections, latp_interval_heur, lonp_interval_heur, analysis_parameters, TransverseAspect, proj_pole_positions_list, output );

                //Oblique aspect
                if ( analysis_parameters.analyze_oblique_aspect )
                        createOptimalLatPLonPPositions ( *p_pl_reference, *i_projections, latp_interval_heur, lonp_interval_heur, analysis_parameters, ObliqueAspect, proj_pole_positions_list, output );


                //Test, if some singular points has been found
                bool singular_points_found = false;

                //Set pointers to processed non singular sets
                Container <Point3DGeographic <T> *> *p_pl_reference_non_sing = p_pl_reference;
                Container <Node3DCartesian <T> *> *p_nl_test_non_sing = &nl_test;

                //Create temporary containers for non singular points: containers of points
                Container <Node3DCartesian <T> *> nl_test_non_sing;
                Container <Point3DGeographic <T> *> pl_reference_non_sing;

                //Non singular pairs
                typename TDevIndexPairs <T>::Type non_singular_pairs;
                TIndexList non_singular_points;

                //Pointer to meridians and parallels
                typename TMeridiansList <T> ::Type * p_meridians_non_sing = &meridians;
                typename TParallelsList <T> ::Type * p_parallels_non_sing = &parallels;

                //Create temporary meridians and parallels
                typename TMeridiansList <T> ::Type meridians_non_sing;
                typename TParallelsList <T> ::Type parallels_non_sing;

                //Process all found positions
                for ( unsigned int i = 0; i < proj_pole_positions_list.size(); i++ )
                {
                        //Set projection parameters: cartographic pole
                        ( *i_projections )->setCartPole ( proj_pole_positions_list[i].cart_pole );
                        ( *i_projections )->setLat0 ( proj_pole_positions_list[i].lat0 );

                        //Try to remove singular points to prevent a throwing of sample only if  cartographic pole coordinates latp, lonp change
                        if ( ( i == 0 ) || ( i > 0 ) && ( proj_pole_positions_list[i].cart_pole != proj_pole_positions_list[i - 1].cart_pole ) )
                        {
                                //Set pointers to old sets (they do not contain singular data)
                                p_pl_reference_non_sing = p_pl_reference;
                                p_nl_test_non_sing = &nl_test;

                                //Remove singular points: empty containers
                                nl_test_non_sing.clear(); pl_reference_non_sing.clear(); non_singular_pairs.clear();
                                removeSingularPoints ( *p_nl_test_non_sing, *p_pl_reference_non_sing, *i_projections, nl_test_non_sing, pl_reference_non_sing, non_singular_pairs );

                                //Singular points found
                                if ( nl_test.size() != nl_test_non_sing.size() )
                                {
                                        //Set flag to true, used for all samples using non-singular sets
                                        singular_points_found = true;

                                        //Create copy of meridians / parallels
                                        meridians_non_sing = meridians;
                                        parallels_non_sing = parallels;

                                        //Correct meridians and parallels
                                        correctMeridiansAndParrallels <T> ( meridians_non_sing, parallels_non_sing, non_singular_pairs );

                                        //Convert non singular pairs to index list: indices will be printed in output
                                        non_singular_points.clear();
                                        std::transform ( non_singular_pairs.begin(), non_singular_pairs.end(), std::back_inserter ( non_singular_points ), getSecondElementInPair() );

                                        //Set pointers to non-singular meridians and parallels
                                        p_meridians_non_sing = &meridians_non_sing;
                                        p_parallels_non_sing = &parallels_non_sing;

                                        //Set pointers to newly created non-singular sets
                                        p_nl_test_non_sing = &nl_test_non_sing;
                                        p_pl_reference_non_sing = &pl_reference_non_sing;
                                }

                                //Singular points did not find
                                else
                                {
                                        //Set flag to false
                                        singular_points_found = false;

                                        //Initialize non singular indices: all indices are valid
                                        non_singular_points.clear();

                                        for ( unsigned int j = 0; j < p_pl_reference->size(); j++ ) non_singular_points.push_back ( j );
                                }
                        }

                        //Compute analysis
                        unsigned int created_samples = 0;

                        try
                        {
                                //Analyze normal aspect
                                Sample <T> analyzed_sample;

                                computeAnalysisForOneSample ( *p_nl_test_non_sing, *p_pl_reference_non_sing, meridians, parallels, faces_test, *i_projections, analysis_parameters, analyzed_sample, singular_points_found, created_samples, output );

                                //Add result to the list
                                if ( total_created_and_analyzed_samples_projection ) sl.push_back ( analyzed_sample );
                        }

                        //Throw exception
                        catch ( Error & error )
                        {
                                if ( analysis_parameters.print_exceptions )
                                        error.printException();
                        }

                        //Increment amount of created and thrown samples
                        if ( created_samples == 0 ) total_created_or_thrown_samples ++;
                        else
                        {
                                total_created_and_analyzed_samples_projection += created_samples;
                                total_created_or_thrown_samples += created_samples;
                        }

                        //Print "." for every 500-th sample to the log
                        if ( total_created_or_thrown_samples % 500 == 0 )
                        {
                                std::cout.flush();
                                std::cout << ".";
                        }
                }

                //Print successfully analyzed samples for one cartographic projection
                *output << " [" << total_created_and_analyzed_samples_projection << " created]" << std::endl;
        }
}


template <typename T>
void CartAnalysis::computeAnalysisForAllSamplesSim ( Container <Sample <T> > &sl, Container <Projection <T> *> &pl, Container <Node3DCartesian <T> *> &nl_test, Container <Point3DGeographic <T> *> &pl_reference,
                typename TMeridiansList <T> ::Type meridians, typename TParallelsList <T> ::Type parallels, const Container <Face <T> *> &faces_test, TAnalysisParameters <T> & analysis_parameters,
                unsigned int & total_created_or_thrown_samples, std::ostream * output )
{
        //Find mimum using the Simplex method ( Nelder-Mead algorithm )
        const unsigned int m = nl_test.size();

        //Total successfully computed analysis for one cartographic projection
        unsigned int total_created_and_analyzed_samples_projection = 0;

        //Create sample for analyzed projection from command line and set flag for this sample
        if ( analysis_parameters.analyzed_projections.size() > 0 )
        {
                //Analyze all projections specified in command line
                for ( typename TItemsList<Projection <T> *> ::Type ::iterator i_projections = analysis_parameters.analyzed_projections.begin(); i_projections != analysis_parameters.analyzed_projections.end(); ++i_projections )
                {
                        //Get analyzed projection
                        Projection <T> *analyzed_proj = *i_projections;

                        //List of points using new central meridian redefined in projection file
                        Container <Point3DGeographic <T> *> pl_reference_red;

                        //Reduce lon using a new central meridian redefined in projection file, if necessary
                        if ( ( *i_projections )->getLon0() != 0.0 ) redLon ( pl_reference, ( *i_projections )->getLon0(), pl_reference_red );

                        //Set pointer to processed file: reduced or non reduced
                        Container <Point3DGeographic <T> *> * p_pl_reference = ( ( *i_projections )->getLon0() == 0.0 ? &pl_reference : &pl_reference_red );

                        //Create temporary containers for non singular points
                        Container <Node3DCartesian <T> *> nl_test_non_sing;
                        Container <Point3DGeographic <T> *> pl_reference_non_sing;

                        typename TDevIndexPairs <T>::Type non_singular_pairs;
                        TIndexList non_singular_points;

                        //Initialize non singular indices
                        for ( unsigned int i = 0; i < p_pl_reference->size(); i++ ) non_singular_points.push_back ( i );

                        //Set pointer to processed file: with or wihout singular points
                        Container <Node3DCartesian <T> *> *p_nl_test = &nl_test;

                        //Remove singular points to prevent throwing a sample
                        bool singular_points_found = false;
                        removeSingularPoints ( *p_nl_test, *p_pl_reference, *i_projections, nl_test_non_sing, pl_reference_non_sing, non_singular_pairs );

                        //Some singular points have been found
                        if ( nl_test.size() != nl_test_non_sing.size() )
                        {
                                //Set pointers to files without singular points
                                p_nl_test = &nl_test_non_sing;	p_pl_reference = &pl_reference_non_sing;

                                //Set flag to true, used for a sample using non-singular sets
                                singular_points_found = true;

                                //Correct meridians and parallels
                                correctMeridiansAndParrallels <T> ( meridians, parallels, non_singular_pairs );

                                //Convert non singular pairs to index list: indices will be printed in output
                                non_singular_points.clear();
                                std::transform ( non_singular_pairs.begin(), non_singular_pairs.end(), std::back_inserter ( non_singular_points ), getSecondElementInPair() );
                        }

                        //Create sample
                        Sample <T> analyzed_sample;

                        //Compute analysis
                        try
                        {
                                ( void ) computeAnalysisForOneSample ( *p_nl_test, *p_pl_reference, meridians, parallels, faces_test, analyzed_proj, analysis_parameters, analyzed_sample, singular_points_found, total_created_and_analyzed_samples_projection, output );
                        }

                        //Throw error
                        catch ( Error & error )
                        {
                                if ( analysis_parameters.print_exceptions ) error.printException();
                        }

                        //Sample with analyzed projection has been successfully created (not thrown by the heuristic)
                        if ( total_created_and_analyzed_samples_projection > 0 ) sl [sl.size() - 1].setAnalyzedProjectionSample ( true );
                }

                if ( total_created_and_analyzed_samples_projection == 0 ) throw ErrorBadData ( "ErrorBadData: no analyzed projection has been used because of dissimilarity.", "Analysis has been stopped." );
        }

        //Process all cartographic projections from the list one by one
        for ( typename TItemsList <Projection <T> *> ::Type ::const_iterator i_projections = pl.begin(); i_projections != pl.end(); ++ i_projections )
        {
                //Get limits of the cartographic pole latitude and longitude: some projections are defined only in normal position
                total_created_and_analyzed_samples_projection = 0;

                //Get defined radius
                const T R_def = ( *i_projections )->getR();

                //Print actual projection name to the log
                std::cout << ( *i_projections ) -> getProjectionName() << ": ";
                *output << ( *i_projections ) -> getProjectionName() << ": ";

                const TMinMax <T> lon_interval ( ( * std::min_element ( pl_reference.begin(), pl_reference.end(), sortPointsByLon () ) )->getLon(),
                                                 ( * std::max_element ( pl_reference.begin(), pl_reference.end(), sortPointsByLon () ) )->getLon() );
                const TMinMax <T> lat_interval ( ( * std::min_element ( pl_reference.begin(), pl_reference.end(), sortPointsByLat () ) )->getLat(),
                                                 ( * std::max_element ( pl_reference.begin(), pl_reference.end(), sortPointsByLat () ) )->getLat() );

                //Get both latp and lonp intervals: lonp intervals are set to the moved central meridian (further must be reduced)
                TMinMax <T> latp_interval_heur = ( *i_projections )->getLatPIntervalH ( lat_interval );
                TMinMax <T> lonp_interval_heur = ( *i_projections )->getLonPIntervalH ( lon_interval );
                TMinMax <T> lat0_interval = ( *i_projections )->getLat0Interval();

                //Create sample
                Sample <T> best_sample;

                //Create matrix
                const unsigned short dim = 5;
                Matrix <T> Y ( 2 * m, 1 );

                try
                {
                        //Compute initial R value
                        unsigned int total_samples_test = 0;
                        Sample <T> sample_test;
                        TAnalysisParameters <T> analysis_parameters_test ( false );
                        analysis_parameters_test.analysis_type.a_helt = true;

                        //Use the cartometric analysis to set the initial value of the Earth radius: use the similarity transformation
                        CartAnalysis::computeAnalysisForOneSample ( nl_test, pl_reference, meridians, parallels, faces_test, *i_projections, analysis_parameters_test, sample_test, false, total_samples_test, output );

                        //Get initialize Earth radius using the similarity transformation
                        const T R_init = ( *i_projections )->getR() / sample_test.getScaleHelT();

                        //Create simplex matrices
                        Matrix <T> XMIN ( 1, dim ), DX ( 1, dim );

                        //Initialize intervals: common values for all aspects
                        XMIN ( 0, 0 ) = R_init * 0.99;
                        XMIN ( 0, 3 ) = lat0_interval.min_val;

                        DX ( 0, 0 ) = 0.02 * R_init;
                        DX ( 0, 3 ) = lat0_interval.max_val - lat0_interval.min_val;

                        //Analyze normal aspect
                        if ( ( analysis_parameters.analyze_normal_aspect ) && ( ( *i_projections ) -> getCartPole().getLat() == MAX_LAT ) )
                        {
                                //Set values for testing
                                unsigned short iterations_tot = 0, effic = 0;
                                T cost_tot = 0, cost_good = 0;
                                srand ( ( unsigned ) time ( 0 ) );

                                //Start time
                                time_t start, end;
                                time ( &start );

                                for ( unsigned int j = 0; j < 300; j++ )
                                {
                                        //std::cout << "test\n";
                                        unsigned short iterations = 0;

                                        //Create matrices
                                        Matrix <T> W ( 2 * m, 2 * m, 0.0, 1 ), V ( dim + 1, 1 );

                                        //Correct values for the normal aspect
                                        XMIN ( 0, 1 ) = MAX_LAT;
                                        XMIN ( 0, 2 ) = 0.0;
                                        XMIN ( 0, 4 ) = lon_interval.min_val;

                                        //*****Start
                                        //XMIN ( 0, 4 ) = 57;
                                        //XMIN ( 0, 4 ) = 60.14;
                                        //XMIN ( 0, 4 ) = 16.43;
                                        //XMIN ( 0, 4 ) = MIN_LON;
                                        /*
                                        T k = 0.01;
                                        T lon_min = MIN_LON + ((1 - k )*(360 )*rand()/(RAND_MAX + 1.0));
                                        T lat0_min = 0 + ((1 - k )*(90 )*rand()/(RAND_MAX + 1.0));
                                        XMIN ( 0, 3 ) = lat0_min;
                                        XMIN ( 0, 4 ) = lon_min;
                                        std::cout << lat0_min << "   " << lon_min << '\n';
                                        //***** End
                                        */

                                        DX ( 0, 1 ) = 0.0;
                                        DX ( 0, 2 ) = 0.0;
                                        DX ( 0, 4 ) = lon_interval.max_val - lon_interval.min_val;


                                        //*****Start
                                        //DX ( 0, 3 ) = 0;
                                        //DX ( 0, 4 ) = 0;
                                        //DX ( 0, 3 ) = 90;
                                        //DX ( 0, 4 ) = 360;
                                        /*
                                        DX ( 0, 3 ) = k * ( 90 );
                                        DX ( 0, 4 ) = k * ( 360 );
                                        //*****End
                                        */

                                        //Initialize random simplex
                                        Matrix <T> X = SimplexMethod::createRandSimplex ( XMIN, DX );

                                        //Store projection properties before analysis
                                        const Point3DGeographic <T> cart_pole = ( *i_projections )->getCartPole();
                                        const T lat0 = ( *i_projections )->getLat0();
                                        const T lon0 = ( *i_projections )->getLon0();
                                        const T dx = ( *i_projections )->getDx();
                                        const T dy = ( *i_projections )->getDy();

                                        //Find minimum using a simplex method
                                        Matrix <T> XBest = SimplexMethod::NelderMead ( FAnalyzeProjV2S <T> ( nl_test, pl_reference, meridians, parallels, faces_test, *i_projections, R_def, analysis_parameters, NormalAspect, best_sample,
                                                           total_created_and_analyzed_samples_projection, output ), W, X, Y, V, iterations, 1.0e-10, 500, output );

                                        //Add result to the list
                                        if ( total_created_and_analyzed_samples_projection )
                                        {
                                                //Test, if X inside interval lon0
                                                if ( ( ( ( lonp_interval_heur.min_val <= lonp_interval_heur.max_val ) /*&& ( XR ( 0, 4 ) >= std::min ( 0.0, lon_interval.max_val ) ) && ( XBest ( 0, 4 ) <= lon_interval.max_val )*/ ) || ( ( lonp_interval_heur.min_val > lonp_interval_heur.max_val ) /* && ( XBest ( 0, 4 ) >= MIN_LON ) && ( XR ( 0, 4 ) <= lon_interval.min_val )*/ ) ) &&
                                                                ( ( XBest ( 0, 3 ) >= lat0_interval.min_val ) && ( XBest ( 0, 3 ) <= lat0_interval.max_val ) ) )
                                                {
                                                        //Set properties of the sample
                                                        best_sample.setR ( XBest ( 0, 0 ) );
                                                        best_sample.setLatp ( MAX_LAT );
                                                        best_sample.setLonp ( 0.0 );
                                                        best_sample.setLat0 ( XBest ( 0, 3 ) );
                                                        best_sample.setLon0 ( XBest ( 0, 4 ) );

                                                        //Add sample to the list
                                                        sl.push_back ( best_sample );
                                                }

                                                else total_created_and_analyzed_samples_projection--;
                                        }

                                        //Restore projection properties after analysis
                                        ( *i_projections )->setCartPole ( cart_pole );
                                        ( *i_projections )->setLat0 ( lat0 );
                                        ( *i_projections )->setLat0 ( lon0 );
                                        ( *i_projections )->setDx ( dx );
                                        ( *i_projections )->setDy ( dy );

                                        //Compute result
                                        iterations_tot += iterations;
                                        cost_tot += norm ( V );

                                        
                                        //Test result
                                        //if ( best_sample.getHomotheticTransformationRatio() < 5.0e6)
                                        if ( best_sample.getHomotheticTransformationRatio() < 5.0e4 )
                                        {
                                                effic ++;
                                        	cost_good += norm ( V );
                                        }

                                        //Bad result
                                        else
                                        {
                                                //*output << "Bad convergence, lat0_min: " << lat0_min;
                                        	//*output << "Bad convergence, lon_min: " << lon_min;
                                                std::cout << "Bad convergence \n";

                                                X.print ( output );
                                                X.print();
                                        }
                                        
                                        //std::cout << j << " ";

                                }


                                //Time difference
                                time ( &end );
                                float time_diff = difftime ( end, start );

                                //Print results
                                *output << "***** RESULTS ***** \n" << '\n';
                                *output << "Efficiency: " << effic << '\n';
                                *output << "Iterations: " << iterations_tot << '\n';
                                *output << "Cost good: " << cost_good << '\n';
                                *output << "Cost total: " << cost_tot << '\n';
                                *output << "Time [s]: " << time_diff << '\n';

                        }

                        Point3DGeographic <T> cart_pole = ( *i_projections ) -> getCartPole();

                        //Analyze transverse aspect: lonp, lat0
                        if ( ( analysis_parameters.analyze_transverse_aspect ) && ( ( *i_projections ) -> getCartPole().getLat() == 0.0 || ( *i_projections ) -> getCartPole().getLat() == MAX_LAT ) &&
                                        ( ( *i_projections ) -> getLatPInterval(). min_val != ( *i_projections ) -> getLatPInterval().max_val ) )
                        {
                                unsigned short iterations = 0;

                                //Create matrices
                                Matrix <T> W ( 2 * m, 2 * m, 0.0, 1 ), V ( dim + 1, 1 );

                                //Correct values for the transverse aspect
                                XMIN ( 0, 1 ) = 0.0;
                                XMIN ( 0, 2 ) = ( lonp_interval_heur.min_val < lonp_interval_heur.max_val ? lonp_interval_heur.min_val : MIN_LON );
                                XMIN ( 0, 4 ) = 0.0;

                                DX ( 0, 1 ) = 0.0;
                                DX ( 0, 2 ) = ( lonp_interval_heur.min_val < lonp_interval_heur.max_val ? lonp_interval_heur.max_val - lonp_interval_heur.min_val : 2 * MAX_LON );
                                DX ( 0, 4 ) = 0.0;

                                //Initialize random simplex
                                Matrix <T> X = SimplexMethod::createRandSimplex ( XMIN, DX );

                                //Assign created samples amount
                                const unsigned int total_created_and_analyzed_samples_projection_before = total_created_and_analyzed_samples_projection;

                                //Store projection properties before analysis
                                const Point3DGeographic <T> cart_pole = ( *i_projections )->getCartPole();
                                const T lat0 = ( *i_projections )->getLat0();
                                const T lon0 = ( *i_projections )->getLon0();
                                const T dx = ( *i_projections )->getDx();
                                const T dy = ( *i_projections )->getDy();

                                //Find minimum using a simplex method
                                Matrix <T> XBest = SimplexMethod::NelderMead ( FAnalyzeProjV2S <T> ( nl_test, pl_reference, meridians, parallels, faces_test, *i_projections, R_def, analysis_parameters, TransverseAspect, best_sample,
                                                   total_created_and_analyzed_samples_projection, output ), W, X, Y, V, iterations, 1.0e-8, 500, output );

                                //Add result to the list
                                if ( total_created_and_analyzed_samples_projection_before <  total_created_and_analyzed_samples_projection )
                                {
                                        //Test, if X inside intervals:  lat0, lonp
                                        if ( ( ( lonp_interval_heur.min_val <= lonp_interval_heur.max_val ) && ( XBest ( 0, 2 ) >= lonp_interval_heur.min_val ) && ( XBest ( 0, 2 ) <= lonp_interval_heur.max_val )  || ( ( lonp_interval_heur.min_val > lonp_interval_heur.max_val ) && ( XBest ( 0, 2 ) >= MIN_LON ) && ( XBest ( 0, 2 ) <= lonp_interval_heur.max_val ) ) ) &&
                                                        ( ( XBest ( 0, 3 ) >= lat0_interval.min_val ) && ( XBest ( 0, 3 ) <= lat0_interval.max_val ) ) )
                                        {
                                                //Set properties of the sample
                                                best_sample.setR ( XBest ( 0, 0 ) );
                                                best_sample.setLatp ( 0.0 );
                                                best_sample.setLonp ( XBest ( 0, 2 ) );
                                                best_sample.setLat0 ( XBest ( 0, 3 ) );
                                                best_sample.setLon0 ( XBest ( 0, 4 ) );

                                                //Add sample to the list
                                                sl.push_back ( best_sample );
                                        }

                                        else total_created_and_analyzed_samples_projection--;
                                }

                                //Restore projection properties after analysis
                                ( *i_projections )->setCartPole ( cart_pole );
                                ( *i_projections )->setLat0 ( lat0 );
                                ( *i_projections )->setLat0 ( lon0 );
                                ( *i_projections )->setDx ( dx );
                                ( *i_projections )->setDy ( dy );
                        }

                        //Analyze oblique aspect: latp, lonp, lat0
                        if ( ( analysis_parameters.analyze_oblique_aspect ) && ( ( *i_projections ) -> getCartPole().getLat() != 0.0 || ( *i_projections ) -> getCartPole().getLat() == MAX_LAT ) &&
                                        ( ( *i_projections ) -> getLatPInterval(). min_val != ( *i_projections ) -> getLatPInterval().max_val ) )
                        {

                                //Set values for testing
                                unsigned short iterations_tot = 0, effic = 0;
                                T cost_tot = 0, cost_good = 0;
                                srand ( ( unsigned ) time ( 0 ) );

                                //Start time
                                time_t start, end;
                                time ( &start );

                                for ( unsigned int j = 0; j < 300; j++ )
                                {
                                        unsigned short iterations = 0;

                                        //Create matrices
                                        Matrix <T> W ( 2 * m, 2 * m, 0.0, 1 ), V ( dim + 1, 1 );

                                        XMIN ( 0, 1 ) = latp_interval_heur.min_val;
                                        XMIN ( 0, 2 ) = ( lonp_interval_heur.min_val < lonp_interval_heur.max_val ? lonp_interval_heur.min_val : MIN_LON );
                                        XMIN ( 0, 4 ) = 0.0;


                                        //*****Start
                                        XMIN ( 0, 1 ) = MIN_LAT;
                                        XMIN ( 0, 2 ) = MIN_LON;
                                        /*
                                        T k = 0.01;
                                        T lon_min = MIN_LON + ((1 - k )*(360 )*rand()/(RAND_MAX + 1.0));
                                        T lat_min = MIN_LAT + ((1 - k )*(180 )*rand()/(RAND_MAX + 1.0));
                                        T lat0_min = 0 + ((1 - k )*(90 )*rand()/(RAND_MAX + 1.0));
                                        XMIN ( 0, 1 ) = lat_min;
                                        XMIN ( 0, 2 ) = lon_min;
                                        std::cout << lat_min << "   " << lon_min  << "  " << lat0_min << '\n';
                                        //***** End
                                        */

                                        DX ( 0, 1 ) = latp_interval_heur.max_val - latp_interval_heur.min_val;
                                        DX ( 0, 2 ) = ( lonp_interval_heur.min_val < lonp_interval_heur.max_val ? lonp_interval_heur.max_val - lonp_interval_heur.min_val : 2 * MAX_LON );
                                        DX ( 0, 4 ) = 0.0;


                                        //*****Start
                                        DX ( 0, 1 ) = 180.0;
                                        DX ( 0, 2 ) = 360.0;
                                        //DX ( 0, 2 ) = 90.0;
                                        /*
                                        DX ( 0, 1 ) = k * ( 180 );
                                        DX ( 0, 2 ) = k * ( 360 );
                                        DX ( 0, 3 ) = k * ( 90 );
                                        //*****End
                                        */

                                        //Initialize random simplex
                                        Matrix <T> X = SimplexMethod::createRandSimplex ( XMIN, DX );

                                        //Assign created samples amount
                                        const unsigned int total_created_and_analyzed_samples_projection_before = total_created_and_analyzed_samples_projection;

                                        //Store projection properties before analysis
                                        const Point3DGeographic <T> cart_pole = ( *i_projections )->getCartPole();
                                        const T lat0 = ( *i_projections )->getLat0();
                                        const T lon0 = ( *i_projections )->getLon0();
                                        const T dx = ( *i_projections )->getDx();
                                        const T dy = ( *i_projections )->getDy();

                                        //Remember old values
                                        T lat0_init = 0.5 * ( lat0_interval.min_val + lat0_interval.max_val );
                                        T lon0_init = lon0;

                                        //Find minimum usinf a simplex method
                                        Matrix <T> XBest = SimplexMethod::NelderMead ( FAnalyzeProjV2S <T> ( nl_test, pl_reference, meridians, parallels, faces_test, *i_projections, R_def, analysis_parameters, ObliqueAspect, best_sample,
                                                           total_created_and_analyzed_samples_projection, output ), W, X, Y, V, iterations, 1.0e-8, 500, output );

                                        //Add result to the list
                                        if ( total_created_and_analyzed_samples_projection_before <  total_created_and_analyzed_samples_projection )
                                        {
                                                //Test, if X inside intervals:  latp, lat0, lonp
                                                //if ( ( XBest ( 0, 1 ) >= latp_interval_heur.min_val ) && ( XBest ( 0, 1 ) <= latp_interval_heur.max_val ) && ( ( ( lonp_interval_heur.min_val <= lonp_interval_heur.max_val ) && ( XBest ( 0, 2 ) >= lonp_interval_heur.min_val ) && ( X ( 0, 2 ) <= lonp_interval_heur.max_val ) )  || ( ( lonp_interval_heur.min_val > lonp_interval_heur.max_val ) &&
                                                //                 ( XBest ( 0, 2 ) >= MIN_LON ) && ( XBest ( 0, 2 ) <= lonp_interval_heur.max_val ) ) ) && ( ( XBest ( 0, 3 ) >= lat0_interval.min_val ) && ( XBest ( 0, 3 ) <= lat0_interval.max_val ) ) )
                                                {
                                                        //Set properties of the sample
                                                        best_sample.setR ( XBest ( 0, 0 ) );
                                                        best_sample.setLatp ( XBest ( 0, 1 ) );
                                                        best_sample.setLonp ( XBest ( 0, 2 ) );
                                                        best_sample.setLat0 ( XBest ( 0, 3 ) );

                                                        //Add to the list
                                                        sl.push_back ( best_sample );
                                                }

                                                //else total_created_and_analyzed_samples_projection--;
                                        }

                                        //Restore projection properties after analysis
                                        ( *i_projections )->setCartPole ( cart_pole );
                                        ( *i_projections )->setLat0 ( lat0 );
                                        ( *i_projections )->setLat0 ( lon0 );
                                        ( *i_projections )->setDx ( dx );
                                        ( *i_projections )->setDy ( dy );


                                        //Compute result
                                        iterations_tot += iterations;
                                        cost_tot += norm ( V );
                                        
                                        //Test result
                                        if ( best_sample.getHomotheticTransformationRatio() < 3.0e5 )
                                        {
                                                effic ++;
                                        	cost_good += norm ( V );
                                        }

                                        //Bad result
                                        else
                                        {
                                                //*output << "Bad convergence, lat_min: " << lat_min;
                                        	//*output << "Bad convergence, lon_min: " << lon_min;
                                        	//*output << "Bad convergence, lat0_min: " << lat0_min;
                                                std::cout << "Bad convergence \n";

                                                X.print ( output );
                                                X.print();
                                        }
                                        
                                        std::cout << j << " ";

                                }


                                //Time difference
                                time ( &end );
                                float time_diff = difftime ( end, start );

                                //Print results
                                *output << "***** RESULTS ***** \n" << '\n';
                                *output << "Efficiency: " << effic << '\n';
                                *output << "Iterations: " << iterations_tot << '\n';
                                *output << "Cost good: " << cost_good << '\n';
                                *output << "Cost total: " << cost_tot << '\n';
                                *output << "Time [s]: " << time_diff << '\n';

                        }
                }

                //Throw exception, analysis have not been computed
                catch ( Error & error )
                {
                        if ( analysis_parameters. print_exceptions )
                                error.printException();
                }

                //Assign the amount of created samples
                total_created_or_thrown_samples += total_created_and_analyzed_samples_projection;

                //Print successfully analyzed samples for one cartographic projection
                std::cout << " [" << total_created_and_analyzed_samples_projection << " created]" << std::endl;
                *output << " [" << total_created_and_analyzed_samples_projection << " created]" << std::endl;
        }

}




template <typename T>
void CartAnalysis::computeAnalysisForAllSamplesDE ( Container <Sample <T> > &sl, Container <Projection <T> *> &pl, Container <Node3DCartesian <T> *> &nl_test, Container <Point3DGeographic <T> *> &pl_reference,
                typename TMeridiansList <T> ::Type meridians, typename TParallelsList <T> ::Type parallels, const Container <Face <T> *> &faces_test, TAnalysisParameters <T> & analysis_parameters,
                unsigned int & total_created_or_thrown_samples, std::ostream * output )
{
        //Find mimum using the differential evolution algorithm
        const unsigned int m = nl_test.size();

        //Total successfully computed analysis for one cartographic projection
        unsigned int total_created_and_analyzed_samples_projection = 0;

        //Create sample for analyzed projection from command line and set flag for this sample
        if ( analysis_parameters.analyzed_projections.size() > 0 )
        {
                //Analyze all projections specified in command line
                for ( typename TItemsList<Projection <T> *> ::Type ::iterator i_projections = analysis_parameters.analyzed_projections.begin(); i_projections != analysis_parameters.analyzed_projections.end(); ++i_projections )
                {
                        //Get analyzed projection
                        Projection <T> *analyzed_proj = *i_projections;

                        //List of points using new central meridian redefined in projection file
                        Container <Point3DGeographic <T> *> pl_reference_red;

                        //Reduce lon using a new central meridian redefined in projection file, if necessary
                        if ( ( *i_projections )->getLon0() != 0.0 ) redLon ( pl_reference, ( *i_projections )->getLon0(), pl_reference_red );

                        //Set pointer to processed file: reduced or non reduced
                        Container <Point3DGeographic <T> *> * p_pl_reference = ( ( *i_projections )->getLon0() == 0.0 ? &pl_reference : &pl_reference_red );

                        //Create temporary containers for non singular points
                        Container <Node3DCartesian <T> *> nl_test_non_sing;
                        Container <Point3DGeographic <T> *> pl_reference_non_sing;

                        typename TDevIndexPairs <T>::Type non_singular_pairs;
                        TIndexList non_singular_points;

                        //Initialize non singular indices
                        for ( unsigned int i = 0; i < p_pl_reference->size(); i++ ) non_singular_points.push_back ( i );

                        //Set pointer to processed file: with or wihout singular points
                        Container <Node3DCartesian <T> *> *p_nl_test = &nl_test;

                        //Remove singular points to prevent throwing a sample
                        bool singular_points_found = false;
                        removeSingularPoints ( *p_nl_test, *p_pl_reference, *i_projections, nl_test_non_sing, pl_reference_non_sing, non_singular_pairs );

                        //Some singular points have been found
                        if ( nl_test.size() != nl_test_non_sing.size() )
                        {
                                //Set pointers to files without singular points
                                p_nl_test = &nl_test_non_sing;	p_pl_reference = &pl_reference_non_sing;

                                //Set flag to true, used for a sample using non-singular sets
                                singular_points_found = true;

                                //Correct meridians and parallels
                                correctMeridiansAndParrallels <T> ( meridians, parallels, non_singular_pairs );

                                //Convert non singular pairs to index list: indices will be printed in output
                                non_singular_points.clear();
                                std::transform ( non_singular_pairs.begin(), non_singular_pairs.end(), std::back_inserter ( non_singular_points ), getSecondElementInPair() );
                        }

                        //Create sample
                        Sample <T> analyzed_sample;

                        //Compute analysis
                        try
                        {
                                ( void ) computeAnalysisForOneSample ( *p_nl_test, *p_pl_reference, meridians, parallels, faces_test, analyzed_proj, analysis_parameters, analyzed_sample, singular_points_found, total_created_and_analyzed_samples_projection, output );
                        }

                        //Throw exception
                        catch ( Error & error )
                        {
                                if ( analysis_parameters.print_exceptions ) error.printException();
                        }

                        //Sample with analyzed projection has been successfully created (not thrown by the heuristic)
                        if ( total_created_and_analyzed_samples_projection > 0 ) sl [sl.size() - 1].setAnalyzedProjectionSample ( true );
                }

                if ( total_created_and_analyzed_samples_projection == 0 ) throw ErrorBadData ( "ErrorBadData: no analyzed projection has been used because of dissimilarity.", "Analysis has been stopped." );
        }

        //Process all cartographic projections from the list one by one
        for ( typename TItemsList <Projection <T> *> ::Type ::const_iterator i_projections = pl.begin(); i_projections != pl.end(); ++ i_projections )
        {
                //Get limits of the cartographic pole latitude and longitude: some projections are defined only in normal position
                total_created_and_analyzed_samples_projection = 0;

                //Get defined radius
                const T R_def = ( *i_projections )->getR();

                //Print actual projection name to the log
                std::cout << ( *i_projections ) -> getProjectionName() << ": ";
                *output << ( *i_projections ) -> getProjectionName() << ": ";

                //Find minimum values
                const TMinMax <T> lon_interval ( ( * std::min_element ( pl_reference.begin(), pl_reference.end(), sortPointsByLon () ) )->getLon(),
                                                 ( * std::max_element ( pl_reference.begin(), pl_reference.end(), sortPointsByLon () ) )->getLon() );
                const TMinMax <T> lat_interval ( ( * std::min_element ( pl_reference.begin(), pl_reference.end(), sortPointsByLat () ) )->getLat(),
                                                 ( * std::max_element ( pl_reference.begin(), pl_reference.end(), sortPointsByLat () ) )->getLat() );

                //Get both latp and lonp intervals: lonp intervals are set to the moved central meridian (further must be reduced)
                TMinMax <T> latp_interval_heur = ( *i_projections )->getLatPIntervalH ( lat_interval );
                TMinMax <T> lonp_interval_heur = ( *i_projections )->getLonPIntervalH ( lon_interval );
                TMinMax <T> lat0_interval = ( *i_projections )->getLat0Interval();

                //Create matrix of intervals: latp, lonp, lat0, lon0
                Matrix <double> A ( 1, 5 ), B ( 1, 5 ), A2 ( 1, 5 ), B2 ( 1, 5 ), X ( 1, 5 ), Y ( 2 * m, 1 );


                //Parameters of the genetic algorithm
                const unsigned int population = 5 * A.cols(), max_iterations = 100000;
                //const T eps = 0.0000001, F = 0.8, CR = 0.5;
                const T eps =   0.000000001, F = 0.8, CR = 0.5;

                try
                {
                        //Compute initial R value
                        unsigned int total_samples_test = 0;
                        Sample <T> sample_test;
                        TAnalysisParameters <T> analysis_parameters_test ( false );
                        analysis_parameters_test.analysis_type.a_helt = true;

                        //Use the cartometric analysis to set the initial value of the Earth radius: use the similarity transformation
                        CartAnalysis::computeAnalysisForOneSample ( nl_test, pl_reference, meridians, parallels, faces_test, *i_projections, analysis_parameters_test, sample_test, false, total_samples_test, output );

                        //Get initialize Earth radius using the similarity transformation
                        const T R_init = ( *i_projections )->getR() / sample_test.getScaleHelT();


                        if ( ( analysis_parameters.analyze_normal_aspect ) && ( ( *i_projections )->getCartPole().getLat() == MAX_LAT ) )
                        {
                                //Create matrices
                                Matrix <T> W ( 2 * m, 2 * m, 0.0, 1 ), V ( 2 * m, 1 );

                                //Store projection properties before analysis
                                const Point3DGeographic <T> cart_pole = ( *i_projections )->getCartPole();
                                const T lat0 = ( *i_projections )->getLat0();
                                const T lon0 = ( *i_projections )->getLon0();

                                //Create intervals
                                A ( 0, 0 ) = 0.1 * R_init; B ( 0, 0 ) = 10 * R_init;
                                A ( 0, 1 ) = MAX_LAT; B ( 0, 1 ) = MAX_LAT;
                                A ( 0, 2 ) = 0.0; B ( 0, 2 ) = 0.0;
                                A ( 0, 3 ) = lat0_interval.min_val; B ( 0, 3 ) = lat0_interval.max_val;

                                //Set lon0 intervals: use heuristic values
                                if ( lonp_interval_heur.min_val <= lonp_interval_heur.max_val )
                                {
                                        A ( 0, 4 ) = lon_interval.min_val; B ( 0, 4 ) = lon_interval.max_val;
                                }

                                //Split in two intervals if: lonp_interval_heur.min_val > lonp_interval_heur.max_val
                                else
                                {
                                        A ( 0, 4 ) = MIN_LON; B ( 0, 4 ) = lon_interval.min_val;
                                }

				T cost_tot = 0, cost_good = 0;
                                unsigned int iterations_tot = 0;
                                unsigned int effic = 0;

                                //Start time
                                time_t start, end;
                                time ( &start );

                                for ( int k = 0; k < 300; k++)
                                {

					//Solve 2 or 3 intervals
					for ( unsigned int i = 0; i < ( lonp_interval_heur.min_val <= lonp_interval_heur.max_val ? 2 : 3 ); i++ )
					{
						const unsigned int total_created_and_analyzed_samples_projection_before = total_created_and_analyzed_samples_projection;
						unsigned int iterations = 0;
						T min_cost = MAX_FLOAT;

						//Create sample
						Sample <T> best_sample;

						//Perform analysis, selection strategy = DEBest2Strategy
						min_cost = DifferentialEvolution::getMinimum ( FAnalyzeProjV2DE <T> ( nl_test, pl_reference, meridians, parallels, faces_test, *i_projections, R_def, analysis_parameters, NormalAspect, best_sample,
								total_created_and_analyzed_samples_projection, output ), A, B, population, max_iterations, eps, F, CR, DEBest2Strategy, W, X, Y, V, iterations );

						//Add result to the list
						if ( total_created_and_analyzed_samples_projection_before < total_created_and_analyzed_samples_projection )
						{
							//Set properties of the sample
							best_sample.setR ( X ( 0, 0 ) );
							best_sample.setLatp ( MAX_LAT );
							best_sample.setLonp ( 0.0 );
							best_sample.setLat0 ( X ( 0, 3 ) );
							best_sample.setLon0 ( X ( 0, 4 ) );

							//Add sample to the list
							sl.push_back ( best_sample );
						}
						/*
						//Process also with lon0 = 0;
						if ( i == 0 )
						{
							A ( 0, 4 ) = 0.0; B ( 0, 4 ) = 0.0;
						}

						//Process the second part of the interval
						else
						{
							A ( 0, 4 ) = lon_interval.max_val; B ( 0, 4 ) = MAX_LON;
						}
						*/
						 //Compute result
                                                iterations_tot += iterations;
                                                cost_tot += min_cost;

                                                //Test result
                                                //if ( best_sample.getHomotheticTransformationRatio() < 5.0e6)
                                                if ( min_cost < 5.0e4 )
                                                {
                                                	effic ++;
                                                	cost_good += min_cost;
                                                }

                                                //Bad result
                                                else
                                                {
                                                	*output << "Bad convergence, latp: ";// << latp_r << ",  lonp:" << lonp_r << '\n';
                                                	std::cout << "Bad convergence, latp: ";// << latp_r << ",  lonp:" << lonp_r << '\n';

                                                	X.print ( output );
                                                	X.print();
                                                }

                                                X.print ( output );
                                                X.print();

                                                std::cout << k << " ";
					}

					//Restore projection properties after analysis
					( *i_projections )->setCartPole ( cart_pole );
					( *i_projections )->setLat0 ( lat0 );
					( *i_projections )->setLat0 ( lon0 );
				}

				//Time difference
                                time ( &end );
                                float time_diff = ( float ) difftime ( end, start );
                                
                                //Print results
                                *output << "***** RESULTS ***** \n" << '\n';
                                *output << "Efficiency: " << effic << '\n';
                                *output << "Iterations: " << iterations_tot << '\n';
                                *output << "Cost good: " << cost_good << '\n';
                                *output << "Cost total: " << cost_tot << '\n';
                                *output << "Time [s]: " << time_diff << '\n';
                        }

                        //Analyze transverse aspect
                        if ( ( analysis_parameters.analyze_transverse_aspect ) && ( ( *i_projections ) -> getCartPole().getLat() == 0.0 || ( *i_projections ) -> getCartPole().getLat() == MAX_LAT ) &&
                                        ( ( *i_projections ) -> getLatPInterval(). min_val != ( *i_projections ) -> getLatPInterval().max_val ) )
                        {
                                //Create matrices
                                Matrix <T> W ( 2 * m, 2 * m, 0.0, 1 ), V ( 2 * m, 1 );

                                //Store projection properties before analysis
                                const Point3DGeographic <T> cart_pole = ( *i_projections )->getCartPole();
                                const T lat0 = ( *i_projections )->getLat0();
                                const T lon0 = ( *i_projections )->getLon0();

                                //Create intervals
                                A ( 0, 0 ) = 0.1 * R_init; B ( 0, 0 ) = 10 * R_init;
                                A ( 0, 1 ) = 0.0; B ( 0, 1 ) = 0.0;
                                A ( 0, 3 ) = lat0_interval.min_val; B ( 0, 3 ) = lat0_interval.max_val;
                                A ( 0, 4 ) = 0.0; B ( 0, 4 ) = 0.0;

                                //Set lonp intervals: use heuristic values
                                if ( lonp_interval_heur.min_val <= lonp_interval_heur.max_val )
                                {
                                        A ( 0, 2 ) = lonp_interval_heur.min_val; B ( 0, 2 ) = lonp_interval_heur.max_val;
                                }

                                //Split in two intervals if: lonp_interval_heur.min_val > lonp_interval_heur.max_val
                                else
                                {
                                        A ( 0, 2 ) = MIN_LON; B ( 0, 2 ) = lonp_interval_heur.max_val;
                                }

                                /*
                                //Cartographic projections has defined a specific position lonp: rewrite a, b
                                if ( ( *i_projections ) -> getCartPole().getLat() != MAX_LAT )
                                {
                                        A ( 1, 0 )  = ( *i_projections ) -> getCartPole().getLon (); B ( 1, 0 )  = A ( 1, 0 ) ;
                                }
                                */

                                //Solve 1 or 2 intervals
                                for ( unsigned int i = 0; i < ( lonp_interval_heur.min_val <= lonp_interval_heur.max_val ? 1 : 2 ); i++ )
                                {
                                        const unsigned int total_created_and_analyzed_samples_projection_before = total_created_and_analyzed_samples_projection;
                                        unsigned int iterations = 0;
                                        T min_cost = MAX_FLOAT;

                                        //Create sample
                                        Sample <T> best_sample;

                                        //Perform analysis, selection strategy = DEBest2Strategy
                                        min_cost = DifferentialEvolution::getMinimum ( FAnalyzeProjV2DE <T> ( nl_test, pl_reference, meridians, parallels, faces_test, *i_projections, R_def, analysis_parameters, NormalAspect, best_sample,
                                                        total_created_and_analyzed_samples_projection, output ), A, B, population, max_iterations, eps, F, CR, DEBest2Strategy, W, X, Y, V, iterations );

                                        //Add result to the list
                                        if ( total_created_and_analyzed_samples_projection_before < total_created_and_analyzed_samples_projection )
                                        {
                                                //Set properties of the sample
                                                best_sample.setR ( X ( 0, 0 ) );
                                                best_sample.setLatp ( 0.0 );
                                                best_sample.setLonp ( X ( 0, 2 ) );
                                                best_sample.setLat0 ( X ( 0, 3 ) );
                                                best_sample.setLon0 ( 0.0 );
                                                //best_sample.setProj ( *i_projections );

                                                //Add sample to the list
                                                sl.push_back ( best_sample );
                                        }

                                        //Process the second part of the interval
                                        A ( 0, 2 ) = lonp_interval_heur.min_val; B ( 0, 2 ) = MAX_LON;
                                }

                                //Restore projection properties after analysis
                                ( *i_projections )->setCartPole ( cart_pole );
                                ( *i_projections )->setLat0 ( lat0 );
                                ( *i_projections )->setLat0 ( lon0 );
                        }

                        //Analyze oblique aspect
                        if ( ( analysis_parameters.analyze_oblique_aspect ) && ( ( *i_projections ) -> getCartPole().getLat() != 0.0 || ( *i_projections ) -> getCartPole().getLat() == MAX_LAT ) &&
                                        ( ( *i_projections ) -> getLatPInterval(). min_val != ( *i_projections ) -> getLatPInterval().max_val ) )
                        {
                                //Create matrices
                                Matrix <T> W ( 2 * m, 2 * m, 0.0, 1 ), V ( 2 * m, 1 );

                                //Store projection properties before analysis
                                const Point3DGeographic <T> cart_pole = ( *i_projections )->getCartPole();
                                const T lat0 = ( *i_projections )->getLat0();
                                const T lon0 = ( *i_projections )->getLon0();

                                //Create intervals
                                A ( 0, 0 ) = 0.1 * R_init; B ( 0, 0 ) = 10 * R_init;
                                A ( 0, 1 ) = latp_interval_heur.min_val; B ( 0, 1 ) = latp_interval_heur.max_val;
                                A ( 0, 3 ) = lat0_interval.min_val; B ( 0, 3 ) = lat0_interval.max_val;
                                A ( 0, 4 ) = 0;
                                B ( 0, 4 ) = 0;

                                //Test
                                A ( 0, 1 ) = MIN_LAT; B ( 0, 1 ) = MAX_LAT;

                                //Set lonp intervals: use heuristic values
                                if ( lonp_interval_heur.min_val <= lonp_interval_heur.max_val )
                                {
                                        A ( 0, 2 ) = lonp_interval_heur.min_val; B ( 0, 2 ) = lonp_interval_heur.max_val;
                                }

                                //Split in two intervals if: lonp_interval_heur.min_val > lonp_interval_heur.max_val
                                else
                                {
                                        A ( 0, 2 ) = MIN_LON; B ( 0, 2 ) = lonp_interval_heur.max_val;
                                }

                                //Test
                                A ( 0, 2 ) = MIN_LON; B ( 0, 2 ) = MAX_LON;

                                /*
                                //Cartographic projections has defined a specific position: rewrite a, b
                                if ( ( ( *i_projections ) -> getCartPole().getLat() != MAX_LAT ) && ( ( *i_projections ) -> getCartPole().getLat() != 0.0 ) )
                                {
                                        A ( 0, 0 ) = ( *i_projections ) -> getCartPole().getLat (); B ( 0, 0 ) = A ( 0, 0 );
                                        A ( 0, 1 ) = ( *i_projections ) -> getCartPole().getLon (); B ( 0, 1 ) = A ( 0, 1 ) ;
                                }
                                */
                                //A ( 0, 0) = 59.71194444; B ( 0, 0 ) = 59.71194444;
                                //A ( 0, 1) = 42.52527778; B ( 0, 1 ) = 42.52527778;

                                T cost_tot = 0, cost_good = 0;
                                unsigned int iterations_tot = 0;
                                unsigned int effic = 0;

                                //Start time
                                time_t start, end;
                                time ( &start );

                                for ( int k = 0; k < 300; k++)
                                {
                                        //Solve 1 or 2 intervals
                                        //for ( unsigned int i = 0; i < ( lonp_interval_heur.min_val <= lonp_interval_heur.max_val ? 1 : 2 ); i++ )
                                        {
                                                const unsigned int total_created_and_analyzed_samples_projection_before = total_created_and_analyzed_samples_projection;
                                                unsigned int iterations = 0;
                                                T min_cost = MAX_FLOAT;

                                                //Create sample
                                                Sample <T> best_sample;

                                                //Perform analysis, selection strategy = DEBest2Strategy
                                                min_cost = DifferentialEvolution::getMinimum ( FAnalyzeProjV2DE <T> ( nl_test, pl_reference, meridians, parallels, faces_test, *i_projections, R_def, analysis_parameters, NormalAspect, best_sample,
                                                                total_created_and_analyzed_samples_projection, output ), A, B, population, max_iterations, eps, F, CR, DEBest2Strategy, W, X, Y, V, iterations );

                                                //Add result to the list
                                                if ( total_created_and_analyzed_samples_projection_before < total_created_and_analyzed_samples_projection )
                                                {
                                                        //Set properties of the sample
                                                        best_sample.setR ( X ( 0, 0 ) );
                                                        best_sample.setLatp ( X ( 0, 1 ) );
                                                        best_sample.setLonp ( X ( 0, 2 ) );
                                                        best_sample.setLat0 ( X ( 0, 3 ) );
                                                        best_sample.setLon0 ( X ( 0, 4 ) );

                                                        //Add sample to the list
                                                        sl.push_back ( best_sample );
                                                }

                                                //Process the second part of the interval
                                                A ( 0, 2 ) = lonp_interval_heur.min_val; B ( 0, 2 ) = MAX_LON;

                                                
                                                //Compute result
                                                iterations_tot += iterations;
                                                cost_tot += min_cost;

                                                //Test result
                                                //if ( best_sample.getHomotheticTransformationRatio() < 5.0e6)
                                                if ( min_cost < 9.0e5 )
                                                {
                                                	effic ++;
                                                	cost_good += min_cost;
                                                }

                                                //Bad result
                                                else
                                                {
                                                	*output << "Bad convergence, latp: ";// << latp_r << ",  lonp:" << lonp_r << '\n';
                                                	std::cout << "Bad convergence, latp: ";// << latp_r << ",  lonp:" << lonp_r << '\n';

                                                	X.print ( output );
                                                	X.print();
                                                }

                                                X.print ( output );
                                                X.print();

                                                std::cout << k << " ";
                                                
                                        }

                                        //Restore projection properties after analysis
                                        ( *i_projections )->setCartPole ( cart_pole );
                                        ( *i_projections )->setLat0 ( lat0 );
                                        ( *i_projections )->setLat0 ( lon0 );
                                }


                                //Time difference
                                time ( &end );
                                float time_diff = ( float ) difftime ( end, start );
                                
                                //Print results
                                *output << "***** RESULTS ***** \n" << '\n';
                                *output << "Efficiency: " << effic << '\n';
                                *output << "Iterations: " << iterations_tot << '\n';
                                *output << "Cost good: " << cost_good << '\n';
                                *output << "Cost total: " << cost_tot << '\n';
                                *output << "Time [s]: " << time_diff << '\n';
                                
                        }
                }

                //Throw exception
                catch ( Error & error )
                {
                        if ( analysis_parameters.print_exceptions )
                                error.printException();
                }

                //Assign the amount of created samples
                total_created_or_thrown_samples += total_created_and_analyzed_samples_projection;

                //Print successfully analyzed samples for one cartographic projection
                std::cout << " [" << total_created_and_analyzed_samples_projection << " created]" << std::endl;
                *output << " [" << total_created_and_analyzed_samples_projection << " created]" << std::endl;
        }
}


template <typename T>
void CartAnalysis::computeAnalysisForAllSamplesMLS ( Container <Sample <T> > &sl, Container <Projection <T> *> &pl, Container <Node3DCartesian <T> *> &nl_test, Container <Point3DGeographic <T> *> &pl_reference,
                typename TMeridiansList <T> ::Type meridians, typename TParallelsList <T> ::Type parallels, const Container <Face <T> *> &faces_test, TAnalysisParameters <T> & analysis_parameters,
                unsigned int & total_created_or_thrown_samples, std::ostream * output )
{
        //Find mimum using the Minimum Least Squares
        const unsigned int m = nl_test.size();

        //Total successfully computed analysis for one cartographic projection
        unsigned int total_created_and_analyzed_samples_projection = 0;

        //Create sample for analyzed projection from command line and set flag for this sample
        if ( analysis_parameters.analyzed_projections.size() > 0 )
        {
                //Analyze all projections specified in command line
                for ( typename TItemsList<Projection <T> *> ::Type ::iterator i_projections = analysis_parameters.analyzed_projections.begin(); i_projections != analysis_parameters.analyzed_projections.end(); ++i_projections )
                {
                        //Get analyzed projection
                        Projection <T> *analyzed_proj = *i_projections;

                        //List of points using new central meridian redefined in projection file
                        Container <Point3DGeographic <T> *> pl_reference_red;

                        //Reduce lon using a new central meridian redefined in projection file, if necessary
                        if ( ( *i_projections )->getLon0() != 0.0 ) redLon ( pl_reference, ( *i_projections )->getLon0(), pl_reference_red );

                        //Set pointer to processed file: reduced or non reduced
                        Container <Point3DGeographic <T> *> * p_pl_reference = ( ( *i_projections )->getLon0() == 0.0 ? &pl_reference : &pl_reference_red );

                        //Create temporary containers for non singular points
                        Container <Node3DCartesian <T> *> nl_test_non_sing;
                        Container <Point3DGeographic <T> *> pl_reference_non_sing;

                        typename TDevIndexPairs <T>::Type non_singular_pairs;
                        TIndexList non_singular_points;

                        //Initialize non singular indices
                        for ( unsigned int i = 0; i < p_pl_reference->size(); i++ ) non_singular_points.push_back ( i );

                        //Set pointer to processed file: with or wihout singular points
                        Container <Node3DCartesian <T> *> *p_nl_test = &nl_test;

                        //Remove singular points to prevent throwing a sample
                        bool singular_points_found = false;
                        removeSingularPoints ( *p_nl_test, *p_pl_reference, *i_projections, nl_test_non_sing, pl_reference_non_sing, non_singular_pairs );

                        //Some singular points have been found
                        if ( nl_test.size() != nl_test_non_sing.size() )
                        {
                                //Set pointers to files without singular points
                                p_nl_test = &nl_test_non_sing;	p_pl_reference = &pl_reference_non_sing;

                                //Set flag to true, used for a sample using non-singular sets
                                singular_points_found = true;

                                //Correct meridians and parallels
                                correctMeridiansAndParrallels <T> ( meridians, parallels, non_singular_pairs );

                                //Convert non singular pairs to index list: indices will be printed in output
                                non_singular_points.clear();
                                std::transform ( non_singular_pairs.begin(), non_singular_pairs.end(), std::back_inserter ( non_singular_points ), getSecondElementInPair() );
                        }

                        //Create sample
                        Sample <T> analyzed_sample;

                        //Compute analysis
                        try
                        {
                                ( void ) computeAnalysisForOneSample ( *p_nl_test, *p_pl_reference, meridians, parallels, faces_test, analyzed_proj, analysis_parameters, analyzed_sample, singular_points_found, total_created_and_analyzed_samples_projection, output );
                        }

                        //Throw error
                        catch ( Error & error )
                        {
                                if ( analysis_parameters.print_exceptions ) error.printException();
                        }

                        //Sample with analyzed projection has been successfully created (not thrown by the heuristic)
                        if ( total_created_and_analyzed_samples_projection > 0 ) sl [sl.size() - 1].setAnalyzedProjectionSample ( true );
                }

                if ( total_created_and_analyzed_samples_projection == 0 ) throw ErrorBadData ( "ErrorBadData: no analyzed projection has been used because of dissimilarity.", "Analysis has been stopped." );
        }

        //Process all cartographic projections from the list one by one
        for ( typename TItemsList <Projection <T> *> ::Type ::const_iterator i_projections = pl.begin(); i_projections != pl.end(); ++ i_projections )
        {
                //Get limits of the cartographic pole latitude and longitude: some projections are defined only in normal position
                total_created_and_analyzed_samples_projection = 0;

                //Get defined radius
                const T R_def = ( *i_projections )->getR();

                //Print actual projection name to the log
                std::cout << ( *i_projections ) -> getProjectionName() << ": ";
                *output << ( *i_projections ) -> getProjectionName() << ": ";

                const TMinMax <T> lon_interval ( ( * std::min_element ( pl_reference.begin(), pl_reference.end(), sortPointsByLon () ) )->getLon(),
                                                 ( * std::max_element ( pl_reference.begin(), pl_reference.end(), sortPointsByLon () ) )->getLon() );
                const TMinMax <T> lat_interval ( ( * std::min_element ( pl_reference.begin(), pl_reference.end(), sortPointsByLat () ) )->getLat(),
                                                 ( * std::max_element ( pl_reference.begin(), pl_reference.end(), sortPointsByLat () ) )->getLat() );

                //Get both latp and lonp intervals: lonp intervals are set to the moved central meridian (further must be reduced)
                TMinMax <T> latp_interval_heur = ( *i_projections )->getLatPIntervalH ( lat_interval );
                TMinMax <T> lonp_interval_heur = ( *i_projections )->getLonPIntervalH ( lon_interval );
                TMinMax <T> lat0_interval = ( *i_projections )->getLat0Interval();

                //Create sample
                Sample <T> best_sample;

                //Create matrix of determined variables
                Matrix <T> X ( 5, 1 ), Y ( 2 * m, 1 );

                try
                {
                        //Compute initial R value
                        unsigned int total_samples_test = 0;
                        Sample <T> sample_test;
                        TAnalysisParameters <T> analysis_parameters_test ( false );
                        analysis_parameters_test.analysis_type.a_helt = true;

                        //Use the cartometric analysis to set the initial value of the Earth radius: use the similarity transformation
                        CartAnalysis::computeAnalysisForOneSample ( nl_test, pl_reference, meridians, parallels, faces_test, *i_projections, analysis_parameters_test, sample_test, false, total_samples_test, output );

                        //Get initialize Earth radius using the similarity transformation
                        const T R_init = ( *i_projections )->getR() / sample_test.getScaleHelT();

                        //Analyze normal aspect
                        if ( ( analysis_parameters.analyze_normal_aspect ) && ( ( *i_projections ) -> getCartPole().getLat() == MAX_LAT ) )
                        {
                                unsigned short iterations = 0;

                                //Create matrices
                                Matrix <T> W ( 2 * m, 2 * m, 0.0, 1 ), V ( 2 * m, 1 );

                                //Assign created samples amount
                                const unsigned int total_created_and_analyzed_samples_projection_before = total_created_and_analyzed_samples_projection;

                                //Store projection properties before analysis
                                const Point3DGeographic <T> cart_pole = ( *i_projections )->getCartPole();
                                const T lat0 = ( *i_projections )->getLat0();
                                const T lon0 = ( *i_projections )->getLon0();
                                const T dx = ( *i_projections )->getDx();
                                const T dy = ( *i_projections )->getDy();

                                //Set values for testing
                                unsigned short iterations_tot = 0, effic = 0;
                                T cost_tot = 0, cost_good = 0;
                                srand ( ( unsigned ) time ( 0 ) );

                                //Compute mean of longitude to initialize lon0
                                T lon_mean = 0;

                                for ( unsigned int i = 0; i < m; i++ )
                                        lon_mean += pl_reference[i]->getLon();

                                lon_mean /= m;

                                //Start time
                                time_t start, end;
                                time ( &start );

                                for ( unsigned int i = 0; i < 300; i++ )
                                {
                                        //Set initial values for matrix: R, latp=90, lonp=0, lat0, lon0, dx, dy
                                        X ( 0, 0 ) = R_init;
                                        X ( 1, 0 ) = MAX_LAT;
                                        X ( 2, 0 ) = 0.0;
                                        X ( 3, 0 ) = 0.5 * ( lat0_interval.min_val + lat0_interval.max_val );
                                        X ( 4, 0 ) =  lon_mean;

                                        //***********Testing start ***************
                                        T lon0_min = -170, lon0_max = 170.0, lat0_max = 85, lat0_min = 0;

                                        //Intervals initialization
                                        T rlon0 = lon0_max - lon0_min + 1;
                                        T rlat0 = lat0_max - lat0_min + 1;

                                        //Random numbers
                                        T lat0_r = lat0_min + int ( rlat0 * rand() / ( RAND_MAX + 1.0 ) );
                                        T lon0_r = lon0_min + int ( rlon0 * rand() / ( RAND_MAX + 1.0 ) );


                                        //Assign lat0, lon0
                                        X ( 3, 0 ) =  lat0_r;
                                        X ( 4, 0 ) =  lon0_r;

                                        //***********Testing end *****************

                                        MinimumLeastSquares::nonLinearLeastSquaresBFGS ( FAnalyzeProjA2 <T> ( nl_test, pl_reference, ( *i_projections ), NormalAspect, analysis_parameters.print_exceptions ), FAnalyzeProjV2 <T> ( nl_test, pl_reference, meridians, parallels, faces_test, *i_projections, R_def, analysis_parameters, NormalAspect, best_sample,
                                                        total_created_and_analyzed_samples_projection, output ), FAnalyzeProjC <double> (), W, X, Y, V, iterations, 1.0e-8, 200, output );

                                        //Add result to the list
                                        if ( total_created_and_analyzed_samples_projection )
                                        {
                                                //Test, if X inside interval lon0
                                                if ( true )
                                                        //if ( ( ( ( lonp_interval_heur.min_val <= lonp_interval_heur.max_val ) /*&& ( XR ( 4, 0 ) >= std::min ( 0.0, lon_interval.max_val ) ) && ( XBest ( 4, 0 ) <= lon_interval.max_val )*/ ) || ( ( lonp_interval_heur.min_val > lonp_interval_heur.max_val ) /* && ( XBest ( 4, 0 ) >= MIN_LON ) && ( XR ( 4, 0 ) <= lon_interval.min_val )*/ ) ) &&
                                                        //                 ( ( X ( 3, 0 ) >= lat0_interval.min_val ) && ( X ( 3, 0 ) <= lat0_interval.max_val ) ) )
                                                {
                                                        //Set properties of the sample
                                                        best_sample.setR ( X ( 0, 0 ) );
                                                        best_sample.setLatp ( MAX_LAT );
                                                        best_sample.setLonp ( 0.0 );
                                                        best_sample.setLat0 ( X ( 3, 0 ) );
                                                        best_sample.setLon0 ( X ( 4, 0 ) );
                                                        //best_sample.setDx ( X ( 5, 0 ) );
                                                        //best_sample.setDy ( X ( 6, 0 ) );

                                                        //Add sample to the list
                                                        sl.push_back ( best_sample );
                                                }

                                                else total_created_and_analyzed_samples_projection--;
                                        }

                                        //Restore projection properties after analysis
                                        ( *i_projections )->setCartPole ( cart_pole );
                                        ( *i_projections )->setLat0 ( lat0 );
                                        ( *i_projections )->setLat0 ( lon0 );
                                        ( *i_projections )->setDx ( dx );
                                        ( *i_projections )->setDy ( dy );

					 //Compute result
                                        iterations_tot += iterations;
                                        cost_tot += norm ( trans ( V ) * W * V );
                                        
                                        //if ( best_sample.getHomotheticTransformationRatio() < 5.0e6)
                                        if ( best_sample.getHomotheticTransformationRatio() < 5.0e4 )
                                        {
                                                effic ++;
                                        	cost_good += norm ( trans ( V ) * W * V );
                                        }

                                        //Bad result
                                        else
                                        {
                                                *output << "Bad convergence, lon0: " << lon0_r  << ",  lat0:" << lat0 << '\n';
                                                std::cout << "Bad convergence, lon0: " << lon0_r  << ",  lat0:" << lat0 << '\n';

                                                X.print ( output );
                                                X.print();
                                        }
                                        
                                        std::cout << i << " ";

                                }

				//Time difference
                                time ( &end );
                                float time_diff = difftime ( end, start );

                                //Print results
                                *output << "***** RESULTS ***** \n" << '\n';
                                *output << "Efficiency: " << effic << '\n';
                                *output << "Iterations: " << iterations_tot << '\n';
                                *output << "Cost good: " << cost_good << '\n';
                                *output << "Cost total: " << cost_tot << '\n';
                                *output << "Time [s]: " << time_diff << '\n';
                        }

                        Point3DGeographic <T> cart_pole = ( *i_projections ) -> getCartPole();

                        //Analyze transverse aspect: lonp, lat0
                        if ( ( analysis_parameters.analyze_transverse_aspect ) && ( ( *i_projections ) -> getCartPole().getLat() == 0.0 || ( *i_projections ) -> getCartPole().getLat() == MAX_LAT ) &&
                                        ( ( *i_projections ) -> getLatPInterval(). min_val != ( *i_projections ) -> getLatPInterval().max_val ) )
                        {
                                unsigned short iterations = 0;

                                //Create matrices
                                Matrix <T> W ( 2 * m, 2 * m, 0.0, 1 ), V ( 2 * m, 1 );

                                //Assign created samples amount
                                const unsigned int total_created_and_analyzed_samples_projection_before = total_created_and_analyzed_samples_projection;

                                //Store projection properties before analysis
                                const Point3DGeographic <T> cart_pole = ( *i_projections )->getCartPole();
                                const T lat0 = ( *i_projections )->getLat0();
                                const T lon0 = ( *i_projections )->getLon0();
                                const T dx = ( *i_projections )->getDx();
                                const T dy = ( *i_projections )->getDy();

                                //Set initial values for matrix: R, latp=0, lonp, lat0, lon0=0, dx, dy
                                X ( 0, 0 ) = R_init;
                                X ( 1, 0 ) = 0.0;
                                X ( 2, 0 ) = 0.5 * ( lonp_interval_heur.min_val + lonp_interval_heur.max_val );
                                X ( 3, 0 ) = 0.5 * ( lat0_interval.min_val + lat0_interval.max_val );
                                X ( 4, 0 ) = 0.0;
                                //X ( 5, 0 ) = ( *i_projections ) ->getDx();
                                //X ( 6, 0 ) = ( *i_projections ) ->getDy();

                                //Compute minimum leat squares
                                MinimumLeastSquares::nonLinearLeastSquaresBFGS ( FAnalyzeProjA2 <T> ( nl_test, pl_reference, ( *i_projections ), TransverseAspect, analysis_parameters.print_exceptions ), FAnalyzeProjV2 <T> ( nl_test, pl_reference, meridians, parallels, faces_test, *i_projections, R_def, analysis_parameters, TransverseAspect, best_sample,
                                                total_created_and_analyzed_samples_projection, output ), FAnalyzeProjC <double> (), W, X, Y, V, iterations, 1.0e-8, 200, output );

                                //Add result to the list
                                if ( total_created_and_analyzed_samples_projection_before <  total_created_and_analyzed_samples_projection )
                                {
                                        //Test, if X inside intervals:  lat0, lonp
                                        if ( ( ( lonp_interval_heur.min_val <= lonp_interval_heur.max_val ) && ( X ( 2, 0 ) >= lonp_interval_heur.min_val ) && ( X ( 2, 0 ) <= lonp_interval_heur.max_val )  || ( ( lonp_interval_heur.min_val > lonp_interval_heur.max_val ) && ( X ( 2, 0 ) >= MIN_LON ) && ( X ( 2, 0 ) <= lonp_interval_heur.max_val ) ) ) &&
                                                        ( ( X ( 3, 0 ) >= lat0_interval.min_val ) && ( X ( 3, 0 ) <= lat0_interval.max_val ) ) )
                                        {
                                                //Set properties of the sample
                                                best_sample.setR ( X ( 0, 0 ) );
                                                best_sample.setLatp ( 0.0 );
                                                best_sample.setLonp ( X ( 2, 0 ) );
                                                best_sample.setLat0 ( X ( 3, 0 ) );
                                                best_sample.setLon0 ( X ( 4, 0 ) );
                                                //best_sample.setDx ( X ( 5, 0 ) );
                                                //best_sample.setDy ( X ( 6, 0 ) );

                                                //Add sample to the list
                                                sl.push_back ( best_sample );
                                        }

                                        else total_created_and_analyzed_samples_projection--;
                                }

                                //Restore projection properties after analysis
                                ( *i_projections )->setCartPole ( cart_pole );
                                ( *i_projections )->setLat0 ( lat0 );
                                ( *i_projections )->setLat0 ( lon0 );
                                ( *i_projections )->setDx ( dx );
                                ( *i_projections )->setDy ( dy );
                        }

                        //Analyze oblique aspect: latp, lonp, lat0
                        if ( ( analysis_parameters.analyze_oblique_aspect ) && ( ( *i_projections ) -> getCartPole().getLat() != 0.0 || ( *i_projections ) -> getCartPole().getLat() == MAX_LAT ) &&
                                        ( ( *i_projections ) -> getLatPInterval(). min_val != ( *i_projections ) -> getLatPInterval().max_val ) )
                        {
                                unsigned short iterations = 0;

                                //Create matrices
                                //Matrix <T> W ( 2 * m, 2 * m, 0.0, 1 ), V ( 2 * m, 1 );

                                //Assign created samples amount
                                const unsigned int total_created_and_analyzed_samples_projection_before = total_created_and_analyzed_samples_projection;

                                //Store projection properties before analysis
                                const Point3DGeographic <T> cart_pole = ( *i_projections )->getCartPole();
                                const T lat0 = ( *i_projections )->getLat0();
                                const T lon0 = ( *i_projections )->getLon0();
                                const T dx = ( *i_projections )->getDx();
                                const T dy = ( *i_projections )->getDy();

                                //Set values for testing
                                unsigned short iterations_tot = 0, effic = 0;
                                T cost_tot = 0, cost_good = 0;
                                srand ( ( unsigned ) time ( 0 ) );

                                //Remember old values
                                T lat0_init = 0.5 * ( lat0_interval.min_val + lat0_interval.max_val );
                                T lon0_init = lon0;
                                //T dx_init = sample_test.getDx();
                                //T dy_init = sample_test.getDy();

                                //Start time
                                time_t start, end;
                                time ( &start );

                                for ( unsigned int i = 0; i < 300; i++ )
                                {
                                        Matrix <T> W ( 2 * m, 2 * m, 0.0, 1 ), V ( 2 * m, 1 );

                                        //Set initial values for matrix: R, latp, lonp, lat0, lon0=0, dx, dy
                                        X ( 0, 0 ) = R_init;
                                        X ( 1, 0 ) = 0.5 * ( latp_interval_heur.min_val + latp_interval_heur.max_val );
                                        X ( 2, 0 ) = 0.5 * ( lonp_interval_heur.min_val + lonp_interval_heur.max_val );
                                        X ( 3, 0 ) = 0.5 * ( lat0_interval.min_val + lat0_interval.max_val );
                                        X ( 4, 0 ) = lon0;

                                        //X ( 5, 0 ) = ( *i_projections ) -> getDx();
                                        //X ( 6, 0 ) = ( *i_projections ) -> getDy();

                                        //*****************Testing start******************
                                        X ( 0, 0 ) = R_init;
                                        X ( 3, 0 ) = lat0_init;
                                        X ( 4, 0 ) = lon0_init;

                                        //X ( 5, 0 ) = dx_init;
                                        //X ( 6, 0 ) = dy_init;


                                        //Intervals initialization
                                        T latp_min = -70, latp_max = 70, lonp_min = -150.0, lonp_max = 150.0, lat0_max = 85, lat0_min = 0;
                                        T rlatp = latp_max - latp_min + 1;
                                        T rlonp = lonp_max - lonp_min + 1;
                                        T rlat0 = lat0_max - lat0_min + 1;

                                        //Random numbers
                                        T latp_r = latp_min + int ( rlatp * rand() / ( RAND_MAX + 1.0 ) );
                                        T lonp_r = lonp_min + int ( rlonp * rand() / ( RAND_MAX + 1.0 ) );
                                        T lat0_r = lat0_min + int ( rlat0 * rand() / ( RAND_MAX + 1.0 ) );

                                        //Assign latp, lonp
                                        X ( 1, 0 ) = latp_r;
                                        X ( 2, 0 ) = lonp_r;
                                        X ( 3, 0 ) = lat0_r;
					X ( 1, 0 ) = -40;
                                        X ( 2, 0 ) = 160;
                                        //*************Testing end *********************

                                        //Compute minimum leat squares
                                        MinimumLeastSquares::nonLinearLeastSquaresBFGS ( FAnalyzeProjA2 <T> ( nl_test, pl_reference, *i_projections, ObliqueAspect, analysis_parameters.print_exceptions ), FAnalyzeProjV2 <T> ( nl_test, pl_reference, meridians, parallels, faces_test, *i_projections, R_def, analysis_parameters, ObliqueAspect, best_sample,
                                                        total_created_and_analyzed_samples_projection, output ), FAnalyzeProjC <double> (), W, X, Y, V, iterations, 1.0e-8, 200, output );

                                        //Add result to the list
                                        if ( total_created_and_analyzed_samples_projection_before <  total_created_and_analyzed_samples_projection )
                                        {
                                                //Test, if X inside intervals:  latp, lat0, lonp
                                                //if ( ( X ( 1, 0 ) >= latp_interval_heur.min_val ) && ( X ( 1, 0 ) <= latp_interval_heur.max_val ) && ( ( ( lonp_interval_heur.min_val <= lonp_interval_heur.max_val ) && ( X ( 2, 0 ) >= lonp_interval_heur.min_val ) && ( X ( 2, 0 ) <= lonp_interval_heur.max_val ) )  || ( ( lonp_interval_heur.min_val > lonp_interval_heur.max_val ) &&
                                                //                 ( X ( 2, 0 ) >= MIN_LON ) && ( X ( 2, 0 ) <= lonp_interval_heur.max_val ) ) ) && ( ( X ( 3, 0 ) >= lat0_interval.min_val ) && ( X ( 3, 0 ) <= lat0_interval.max_val ) ) )
                                                {
                                                        //Set properties of the sample
                                                        best_sample.setR ( X ( 0, 0 ) );
                                                        best_sample.setLatp ( X ( 1, 0 ) );
                                                        best_sample.setLonp ( X ( 2, 0 ) );
                                                        best_sample.setLat0 ( X ( 3, 0 ) );
                                                        best_sample.setLon0 ( X ( 4, 0 ) );
                                                        //best_sample.setDx ( X ( 5, 0 ) );
                                                        //best_sample.setDy ( X ( 6, 0 ) );

                                                        //Add to the list
                                                        sl.push_back ( best_sample );
                                                }

                                                //else total_created_and_analyzed_samples_projection--;
                                        }

                                        //Restore projection properties after analysis
                                        ( *i_projections )->setCartPole ( cart_pole );
                                        ( *i_projections )->setLat0 ( lat0 );
                                        ( *i_projections )->setLat0 ( lon0 );
                                        ( *i_projections )->setDx ( dx );
                                        ( *i_projections )->setDy ( dy );


                                        //Test result
                                        iterations_tot += iterations;
                                        cost_tot += norm ( trans ( V ) * W * V );
                                        
                                        //Correct result
                                        //if ( best_sample.getHomotheticTransformationRatio() < 5.0e6)
                                        if ( best_sample.getHomotheticTransformationRatio() < 3.0e5 )
                                        {
                                                effic ++;
                                        	cost_good += norm ( trans ( V ) * W * V );
                                        }

                                        //Bad result
                                        else
                                        {
                                                *output << "Bad convergence, latp: " << latp_r << ",  lonp:" << lonp_r << ",  lat0:" << lat0 <<  '\n';
                                                std::cout << "Bad convergence, latp: " << latp_r << ",  lonp:" << lonp_r << ",  lat0:" << lat0 << '\n';

                                                X.print ( output );
                                                X.print();
                                        }
                                        
                                        std::cout << i << " ";


                                }

                                //Time difference
                                time ( &end );
                                float time_diff = difftime ( end, start );

                                //Print results
                                *output << "***** RESULTS ***** \n" << '\n';
                                *output << "Efficiency: " << effic << '\n';
                                *output << "Iterations: " << iterations_tot << '\n';
                                *output << "Cost good: " << cost_good << '\n';
                                *output << "Cost total: " << cost_tot << '\n';
                                *output << "Time [s]: " << time_diff << '\n';

                        }
                }

                //Throw exception, analysis have not been computed
                catch ( Error & error )
                {
                        if ( analysis_parameters. print_exceptions )
                                error.printException();
                }

                //Assign the amount of created samples
                total_created_or_thrown_samples += total_created_and_analyzed_samples_projection;

                //Print successfully analyzed samples for one cartographic projection
                std::cout << " [" << total_created_and_analyzed_samples_projection << " created]" << std::endl;
                *output << " [" << total_created_and_analyzed_samples_projection << " created]" << std::endl;
        }

}


template <typename T>
void CartAnalysis::createOptimalLatPLonPPositions ( const Container <Point3DGeographic <T> *> &pl_reference, Projection <T> *proj, const TMinMax <T> &latp_interval_heur, const TMinMax <T> &lonp_interval_heur, const TAnalysisParameters <T> & analysis_parameters,
                const TProjectionAspect & proj_aspect, typename TItemsList <TProjectionPolePosition<T> >::Type & proj_pole_positions_list, std::ostream * output )
{
        //Create list of latp, lonp poitions with respect to complex criteriA (used only first 20% of results)
        //TProjectionAspect = NormalAspect create latp, lonp positions for a normal aspect
        //TProjectionAspect = TransverseAspect = create latp, lonp positions for a transverse aspect
        //TProjectionAspect = ObliqueAspect = create latp, lonp positions for an oblique aspect
        T complex_crit_sum = 0.0;

        const bool lat0_set = ( proj -> getLat0() != 0.0 ) && ( proj -> getLat0() != 45.0 ) ;
        const bool latp_set =   proj -> getCartPole().getLat() != MAX_LAT;
        const bool lonp_set =   proj -> getCartPole().getLon() != 0.0;

        //Set latp for a projection aspect
        T latp = MAX_LAT;																							//Normal apect

        if ( proj_aspect == TransverseAspect ) latp = 0.0;																			//Transverse aspect
        else if ( proj_aspect == ObliqueAspect )  latp = proj -> getLatPInterval().min_val;															//Oblique aspect

        //Load from the configuration file
        if ( latp_set ) latp = proj -> getCartPole().getLat();

        //Set latp_min, latp_max for a projection aspect
        T latp_min = MAX_LAT, latp_max = MAX_LAT;																				//Normal aspect

        if ( proj_aspect == TransverseAspect )																					//Transverse aspect
        {
                latp_min = 0.0; latp_max = latp_min;
        }

        else if ( proj_aspect == ObliqueAspect )																				//Oblique aspect
        {
                latp_min =  proj -> getLatPInterval().min_val; latp_max =  proj -> getLatPInterval().max_val;
        }

        //Set lonp_min, lonp_max for a projection aspect
        T lonp_min = 0.0, lonp_max = 0.0;																					//Normal aspect

        if ( proj_aspect == TransverseAspect || proj_aspect == ObliqueAspect )																	//Transverse or oblique aspect
        {
                lonp_min =  proj -> getLonPInterval().min_val; lonp_max =  proj -> getLonPInterval().max_val;
        }

        //Process normal / transverse / oblique aspect of the map projection
        for ( ; ( proj_aspect == TransverseAspect  ? latp == 0.0 : ( latp >= latp_min ) && ( latp <= latp_max ) ); latp += analysis_parameters.latp_step )
        {
                //Set lonp for a projection aspect
                T lonp = 0.0;																							//Normal aspect

                if ( ( latp != MAX_LAT ) && ( proj_aspect == TransverseAspect  || proj_aspect == ObliqueAspect ) )												//Transverse or oblique aspect
                        lonp = ( lonp_set ) ? proj -> getCartPole().getLon() : proj -> getLonPInterval().min_val ;												//Load from configuration file or use default value

                //lonp = -90.0;
                for ( ; ( latp == MAX_LAT ? lonp == 0.0 : ( lonp >= lonp_min ) && ( lonp <= lonp_max ) ); lonp += analysis_parameters.lonp_step )
                {
                        //Test, if a generated lonp value satisfies a condition of heuristic, for a normal aspect do no test
                        if ( ( proj_aspect == NormalAspect ) && ( latp == MAX_LAT )  ||																//Normal aspect
                                        ( proj_aspect == TransverseAspect  || ( proj_aspect == ObliqueAspect ) && ( ( latp != MAX_LAT ) && ( latp != 0.0 ) ) ) &&						//Oblique aspect ( - normal and - transverse aspects ) or transverse aspect
                                        ( ( lonp_interval_heur.min_val < lonp_interval_heur.max_val ) && ( ( lonp >= lonp_interval_heur.min_val ) && ( lonp <= lonp_interval_heur.max_val ) ) ||		//Inside lonp interval given by heuristic
                                          ( lonp_interval_heur.min_val > lonp_interval_heur.max_val ) && ( ( lonp >= lonp_interval_heur.min_val ) || ( lonp <= lonp_interval_heur.max_val ) ) ) &&		//Outside lonp interval given by heuristic
                                        ( ( latp >= latp_interval_heur.min_val ) && ( latp <= latp_interval_heur.max_val ) ) )
                        {
                                //Set lat0: rememeber old value lat0
                                const T lat0_old = proj -> getLat0();

                                //Process all undistorted meridians for latp, lonp positions
                                for ( T lat0 = ( lat0_set ? lat0_old : proj-> getLat0Interval().min_val ); ( lat0 <= proj -> getLat0Interval().max_val ); lat0 += analysis_parameters.lat0_step )
                                {
                                        //Set lat0 to compute correct Tissot indicatrix
                                        proj->setLat0 ( lat0 );

                                        //Compute coordinates of all geographic points points in sample's projection + complex criterium
                                        const unsigned int n_ref = pl_reference.size();
                                        T lat_lon_mbr [] = { MAX_LAT, MAX_LON, MIN_LAT, MIN_LON };

                                        for ( unsigned int i = 0; i < n_ref; i++ )
                                        {
                                                try
                                                {
                                                        //Get type of the direction
                                                        TTransformedLongtitudeDirection trans_lon_dir = proj->getLonDir();

                                                        //Convert geographic point to oblique position: use a normal direction of converted longitude
                                                        const T lat_trans = CartTransformation::latToLatTrans ( pl_reference [i]->getLat(), pl_reference [i]->getLon(), latp,  lonp );
                                                        const T lon_trans = CartTransformation::lonToLonTrans ( pl_reference [i]->getLat(), pl_reference [i]->getLon(), lat_trans, latp, lonp, trans_lon_dir );

                                                        //Find extreme values
                                                        if ( lat_trans < lat_lon_mbr [0] ) lat_lon_mbr [0] = lat_trans;
                                                        else if ( lat_trans > lat_lon_mbr [2] ) lat_lon_mbr[2] = lat_trans;

                                                        if ( lon_trans < lat_lon_mbr[1] ) lat_lon_mbr[1] = lon_trans;
                                                        else if ( lon_trans > lat_lon_mbr[3] ) lat_lon_mbr[3] = lon_trans;
                                                }

                                                //Throw exception: a conversion error or bad data
                                                catch ( Error & error )
                                                {
                                                        if ( analysis_parameters.print_exceptions ) error.printException ( output );
                                                }
                                        }

                                        //Compute complex criterium
                                        T complex_crit = 0.0, weight_sum = 0.0;

                                        //Compute complex criteria in extreme points
                                        if ( analysis_parameters.perform_heuristic )
                                        {
                                                for ( unsigned int i = 0; i < 2; i += 2 )
                                                {
                                                        //Create temporary point
                                                        Point3DGeographic <T> p_oblique_temp ( lat_lon_mbr[i], lat_lon_mbr[i + 1] );

                                                        T h = 1.0, k = 1.0;

                                                        try
                                                        {
                                                                //Compute Tissot indiatrix parameters
                                                                h = CartDistortion::H ( NUM_DERIV_STEP, &p_oblique_temp, proj, analysis_parameters.print_exceptions );
                                                                k = CartDistortion::K ( NUM_DERIV_STEP, &p_oblique_temp, proj, analysis_parameters.print_exceptions );

                                                                //h =  cos ( proj->getLat0() * M_PI / 180 ) / cos ( p_oblique_temp.getLat() * M_PI / 180 );
                                                                //k =  k;//cos ( proj->getLat0() * 180 / M_PI );

                                                                //Switch h <-> k
                                                                if ( h < k )
                                                                {
                                                                        const T temp = h; h = k; k = temp;
                                                                }
                                                        }

                                                        //Throw exception: can not compute Tissot indicatrix
                                                        catch ( Error & error )
                                                        {
                                                                if ( analysis_parameters.print_exceptions ) 	error.printException ( output );
                                                        }

                                                        //Compute complex criteria
                                                        const T weight = cos ( M_PI / 180.0 *  lat_lon_mbr[i] );
                                                        complex_crit += ( 0.5 * ( fabs ( h - 1.0 ) +  fabs ( k - 1.0 ) )  + h / k - 1.0 ) * weight;
                                                        weight_sum += weight;
                                                }

                                                //Complex criteria
                                                complex_crit /=  weight_sum;
                                                complex_crit_sum += complex_crit;
                                        }

                                        //Create new possible pole position
                                        TProjectionPolePosition <T> latp_lonp ( latp, lonp, lat0, complex_crit );

                                        //Add to the list of possible pole positions
                                        proj_pole_positions_list.push_back ( latp_lonp );

                                        //Projection has set lat0 in a definition file
                                        if ( lat0_set ) break;
                                }

                                //Set old lat0
                                proj->setLat0 ( lat0_old );
                        }

                        //Projection has set lonp in a definition file
                        if ( lonp_set ) break;
                }

                //Projection has set latp in a definition file
                if ( latp_set ) break;
        }

        //Remove inappropriate pole positions
        if ( ( analysis_parameters.perform_heuristic ) && ( proj_pole_positions_list.size() > 10 ) )
        {
                //Erase all worse than 2 *mean( complex_crit )
                proj_pole_positions_list.erase ( std::remove_if ( proj_pole_positions_list.begin(), proj_pole_positions_list.end(), removeProjectionPolePositions <T> ( 2.0 * complex_crit_sum / proj_pole_positions_list.size() ) ), proj_pole_positions_list.end() );

                //Sort all created  positions according to cartographic pole values
                std::sort ( proj_pole_positions_list.begin(), proj_pole_positions_list.end(), sortProjectionPolePositionsByLat () );
        }
}


template <typename T>
void CartAnalysis::findLatPLonPIntervals ( const Container <Point3DGeographic <T> *> &pl_reference, Projection <T> *proj, TMinMax <T> &latp_interval_heur, TMinMax <T> &lonp_interval_heur )
{
        //Find optimal latp and lonp intervals with respect to geographic properties of the analyzed area and a type of the projection
        const TMinMax <T> lon_interval ( ( * std::min_element ( pl_reference.begin(), pl_reference.end(), sortPointsByLon () ) )->getLon(),
                                         ( * std::max_element ( pl_reference.begin(), pl_reference.end(), sortPointsByLon () ) )->getLon() );
        const TMinMax <T> lat_interval ( ( * std::min_element ( pl_reference.begin(), pl_reference.end(), sortPointsByLat () ) )->getLat(),
                                         ( * std::max_element ( pl_reference.begin(), pl_reference.end(), sortPointsByLat () ) )->getLat() );

        //Find, which intervals are covered by the analyzed territory: i1 = (-180, -90), i2 = (-90, 0), i3 = (0,90), i4 = (90, 180)
        bool i1 = false, i2 = false, i3 = false, i4 = false;

        for ( unsigned int i = 0; ( i < pl_reference.size() ) && ( ! ( i1 && i2 && i3 && i4 ) ) ; i++ )
        {
                if ( ( pl_reference [i]->getLon() > MIN_LON ) && ( pl_reference [i]->getLon() < -90.0 ) ) i1 = true;
                else if ( ( pl_reference [i]->getLon() > -90.0 ) && ( pl_reference [i]->getLon() < 0.0 ) ) i2 = true;
                else if ( ( pl_reference [i]->getLon() > 0.0 ) && ( pl_reference [i]->getLon() < 90.0 ) ) i3 = true;
                else if ( ( pl_reference [i]->getLon() > 90.0 ) && ( pl_reference [i]->getLon() < MAX_LON ) ) i4 = true;
        }

        //Test, if analyzed area is small. Up to 2 intervals are covered ( no three nor four intervals covered )
        if ( ( ! ( i1 && i2 && i3 && i4 ) ) && ( ! ( i1 && i2 && i3 ) ) && ( ! ( i2 && i3 && i4 ) ) &&
                        ( ! ( i3 && i4 && i1 ) ) && ( ! ( i4 && i1 && i2 ) ) && ( latp_interval_heur.min_val != MAX_LAT ) )
        {
                //Compute heuristic intervals
                TMinMax <T> latp_interval_oblique = proj->getLatPIntervalH ( lat_interval );
                TMinMax <T> lonp_interval_oblique = proj->getLonPIntervalH ( lon_interval );

                //Switch min and max if itervals i1 and i4 are covered
                if ( i1 && i4 )
                {
                        T temp = lonp_interval_oblique.min_val;
                        lonp_interval_oblique.min_val = lonp_interval_oblique.max_val;
                        lonp_interval_oblique.max_val = temp;
                }

                //Round values: min down, max up, to 10 deg
                lonp_interval_oblique.min_val = ( ( int ) ( lonp_interval_oblique.min_val / 10.0 ) ) * 10.0;
                lonp_interval_oblique.max_val = ( ( int ) ( lonp_interval_oblique.max_val / 10.0 + 0.5 ) ) * 10.0;

                //Assign intervals
                latp_interval_heur = latp_interval_oblique;
                lonp_interval_heur = lonp_interval_oblique;
        }
}



template <typename T>
T CartAnalysis::computeAnalysisForOneSample ( Container <Node3DCartesian <T> *> &nl_test, Container <Point3DGeographic <T> *> &pl_reference, typename TMeridiansList <T> ::Type & meridians, typename TParallelsList <T> ::Type & parallels,
                const Container <Face <T> *> &faces_test, Projection <T> *proj, const TAnalysisParameters <T> & analysis_parameters, Sample <T> &sample_res, bool singular_points_found, unsigned int & created_samples, std::ostream * output )
{
        //Compute all cartometric analysis for one sample
        unsigned int n_nsing = nl_test.size(), n_best = n_nsing;
        bool outliers_found = false;

        //Set pointers to processed non sets
        Container <Point3DGeographic <T> *> * p_pl_reference = &pl_reference;
        Container <Point3DGeographic <T> *> *p_pl_reference_non_sing = p_pl_reference;
        Container <Node3DCartesian <T> *> *p_nl_test_non_sing = &nl_test;

        //Create temporary containers for non singular points: containers of points
        Container <Point3DGeographic <T> *> pl_reference_red;
        Container <Node3DCartesian <T> *> nl_test_non_sing;
        Container <Point3DGeographic <T> *> pl_reference_non_sing;

        //Set pointers to meridians and parallels
        typename TMeridiansList <T> ::Type * p_meridians_non_sing = &meridians;
        typename TParallelsList <T> ::Type * p_parallels_non_sing = &parallels;

        //Create temporary meridians and parallels
        typename TMeridiansList <T> ::Type meridians_non_sing;
        typename TParallelsList <T> ::Type parallels_non_sing;

        //List of singular points
        TIndexList non_singular_points;

        for ( unsigned int j = 0; j < n_nsing; j++ ) non_singular_points.push_back ( j );

        //Reduce lon using a new central meridian redefined in projection file, if necessary
        if ( proj->getLon0() != 0.0 )
        {
                //Reduce lon
                redLon ( pl_reference, proj->getLon0(), pl_reference_red );

                //Set pointer to processed file: reduced or non reduced
                p_pl_reference_non_sing = &pl_reference_red;
        }

        //Remove singular points, store non singular pairs
        typename TDevIndexPairs <T>::Type non_singular_pairs;
        removeSingularPoints ( *p_nl_test_non_sing, *p_pl_reference_non_sing, proj, nl_test_non_sing, pl_reference_non_sing, non_singular_pairs );

        //Set singular point flag
        singular_points_found = false;

        //Singular points found: remove points, correct meridians and parallels
        n_nsing  = nl_test_non_sing.size();

        if ( nl_test.size() != n_nsing )
        {
                //Set flag to true, used for all samples using non-singular sets
                singular_points_found = true;

                //Create copy of meridians / parallels
                meridians_non_sing = meridians; parallels_non_sing = parallels;

                //Correct meridians and parallels
                correctMeridiansAndParrallels <T> ( meridians_non_sing, parallels_non_sing, non_singular_pairs );

                //Convert non singular pairs to index list: indices will be printed in output
                non_singular_points.clear();
                std::transform ( non_singular_pairs.begin(), non_singular_pairs.end(), std::back_inserter ( non_singular_points ), getSecondElementInPair() );

                //Set pointers to non-singular meridians and parallels
                p_meridians_non_sing = &meridians_non_sing; p_parallels_non_sing = &parallels_non_sing;

                //Set pointers to newly created non-singular sets
                p_nl_test_non_sing = &nl_test_non_sing; p_pl_reference_non_sing = &pl_reference_non_sing;
        }

        //Set non-singular points
        sample_res.setNonSingularPointsIndices ( non_singular_points );

        //Create empty list of projected points and transformed points
        Container <Node3DCartesianProjected <T> *> nl_projected;

        //Compute coordinates of all geographic points points in sample's projection and add to the list
        for ( unsigned int i = 0; i < n_nsing; i++ )
        {
                try
                {
                        //Get type of the direction
                        TTransformedLongtitudeDirection trans_lon_dir = proj->getLonDir();

                        //Convert geographic point to oblique position: use a normal direction of converted longitude
                        const T lat_trans = CartTransformation::latToLatTrans ( ( *p_pl_reference_non_sing ) [i]->getLat(), ( *p_pl_reference_non_sing ) [i]->getLon(), proj->getCartPole().getLat(),  proj->getCartPole().getLon() );
                        const T lon_trans =  CartTransformation::lonToLonTrans ( ( *p_pl_reference_non_sing ) [i]->getLat(), ( *p_pl_reference_non_sing ) [i]->getLon(), lat_trans, proj->getCartPole().getLat(), proj->getCartPole().getLon(), trans_lon_dir );

                        //Create temporary point
                        Point3DGeographic <T> p_oblique_temp ( lat_trans, lon_trans );

                        //Create new cartographic point; if math exception, move the point
                        T x = 0.0, y = 0.0;

                        for ( unsigned int j = 0; j < 2; j ++ )
                        {
                                try
                                {
                                        x = CartTransformation::latLonToX ( & p_oblique_temp, proj, analysis_parameters.print_exceptions );
                                        y = CartTransformation::latLonToY ( & p_oblique_temp, proj, analysis_parameters.print_exceptions );

                                        break;
                                }

                                //Throw exception
                                catch ( ErrorMath <T> &error )
                                {
                                        //The point's latitude same as the pole: throw exception
                                        if ( fabs ( lat_trans ) == MAX_LAT )
                                        {
                                                throw;
                                        }

                                        //std::cout << "move";

                                        //Otherwise move geographic cordinates of the point
                                        p_oblique_temp.setLat ( lat_trans + GRATICULE_ANGLE_SHIFT );
                                        p_oblique_temp.setLon ( lon_trans + GRATICULE_ANGLE_SHIFT );
                                }
                        }

                        //Create new point
                        Node3DCartesianProjected <T> *n_projected = new Node3DCartesianProjected <T> ( x, y, 0.0, 0.0, 0.0, 0.0, 0.0, TTissotIndikatrix <T> (), 0.0 );

                        //Uncertainty regions: compute Tissot indikatrix parameters only for perspective samples ( will be faster ) for homothetic and helmert transformations
                        if ( ( analysis_parameters.analysis_type.a_homt || analysis_parameters.analysis_type.a_helt ) && ( analysis_parameters.match_method == MatchTissotIndikatrix ) )
                        {
                                //Create Tissot structure, initialize with default parameters independent on (lat, lon): a = 1, b = 1, Ae = 0
                                TTissotIndikatrix <T> tiss;

                                //Compute Tissot indicatrix parameters for (lat, lon)
                                try
                                {
                                        tiss = CartDistortion::Tiss ( NUM_DERIV_STEP, &p_oblique_temp, proj, analysis_parameters.print_exceptions );
                                }

                                //If impossible to compute Tissot indicatrix parameters for (lat, lon), use default parameters (circle a=1, b=1)
                                catch ( Error & error )
                                {
                                        if ( analysis_parameters.print_exceptions ) error.printException ( output );
                                }

                                //Set Tissot indikatrix parameters for the point
                                n_projected->setTiss ( tiss );
                        }

                        //Create new cartographic point
                        nl_projected.push_back ( n_projected );
                }

                //Throw exception: unspecified exception
                catch ( Error & error )
                {
                        if ( analysis_parameters.print_exceptions ) error.printException ( output );
                }
        }

        //Remove duplicate elements from reference data set (projected points)
        nl_projected.removeDuplicateElements ( nl_projected.begin(), nl_projected.end(),
                                               sortPointsByX (), isEqualPointByPlanarCoordinates <Node3DCartesianProjected <T> *> () );

        //Both datasets do not contain the same amount of points
        if ( n_nsing != nl_projected.size() )
                throw ErrorBadData ( "ErrorBadData: both datasets contain a different number of points. ", "Sample had been thrown..." );

        //Set pointers both to test and projected files with non_singular items
        Container <Node3DCartesian <T> *> * p_nl_test_best = p_nl_test_non_sing;
        Container <Node3DCartesianProjected <T> *> * p_nl_projected_best = &nl_projected;

        //Create temporary containers for k-best fit points: containers of points
        Container <Node3DCartesian <T> *> nl_test_best;
        Container <Node3DCartesianProjected <T> *> nl_projected_best;

        //List of k-best points
        TIndexList k_best_points;

        for ( unsigned int i = 0; i < n_nsing; i++ ) k_best_points.push_back ( i );

        //Set pointers to meridians and parallels
        typename TMeridiansList <T> ::Type * p_meridians_best = p_meridians_non_sing; typename TParallelsList <T> ::Type * p_parallels_best = p_parallels_non_sing;

        //Create temporary meridians and parallels
        typename TMeridiansList <T> ::Type meridians_best; typename TParallelsList <T> ::Type parallels_best;

        //List of k_best pairs and multiplication ratio
        typename TDevIndexPairs <T>::Type min_pairs;

        //Remove outliers
        if ( analysis_parameters.remove_outliers )
        {
                TTransformationKeyHelmert2D <T> min_key;

                //Find k-best fit transformation key using Homothetic transformation
                Transformation2D::findOptimalTransformationKeyIRLS ( *p_nl_test_non_sing, nl_projected, nl_test_best, nl_projected_best, min_key, min_pairs );

                //Any outlier found
                n_best = nl_projected_best.size();

                if ( n_nsing != n_best )
                {
                        //Set flag: outliers found
                        outliers_found = true;

                        //Create copy of meridians and parallels with no outliers
                        meridians_best = *p_meridians_non_sing; parallels_best = *p_parallels_non_sing;

                        //Correct meridians and parallels: remove inappropriate points from points indices of all meridians and parallels
                        correctMeridiansAndParrallels <T> ( meridians_best, parallels_best, min_pairs );

                        //Convert k-best pairs to index list: indices will be printed in output
                        k_best_points.clear();
                        std::transform ( min_pairs.begin(), min_pairs.end(), std::back_inserter ( k_best_points ), getSecondElementInPair() );

                        //Set pointers to meridians and parallels
                        p_meridians_best = &meridians_best; p_parallels_best = &parallels_best;

                        //Set pointers to newly created without outliers
                        p_nl_test_best = &nl_test_best; p_nl_projected_best = &nl_projected_best;
                }
        }

        //Set  k-best points
        sample_res.setKBestPointsIndices ( k_best_points );

        //Compare shape of equator, meridian and north / south pole using turning function, similarity transformation: this is a heuristic throwing unperspective samples
        T sample_cost = MAX_FLOAT;

        if ( ( analysis_parameters.perform_heuristic ) && ( checkSample ( *p_meridians_best, *p_parallels_best, *p_nl_test_best, *p_nl_projected_best, analysis_parameters.heuristic_sensitivity_ratio ) ) ||
                        ( ! analysis_parameters.perform_heuristic ) )
        {
                //Compute multiplication ratio
                float mult_ratio = 2.0 - n_best / ( float ) n_nsing;

                //Set properties for the reulting smple
                sample_res.setProj ( proj );
                sample_res.setR ( proj->getR() );
                sample_res.setLatp ( proj->getCartPole().getLat() );
                sample_res.setLonp ( proj->getCartPole().getLon() );
                sample_res.setLat0 ( proj->getLat0() );
                sample_res.setLon0 ( proj->getLon0() );
                sample_res.setDx ( proj->getDx() );
                sample_res.setDy ( proj->getDy() );
                sample_res.setSingularPointsFound ( singular_points_found );
                sample_res.setOutliersFound ( outliers_found );
                sample_res.setNonSingularPointsIndices ( non_singular_points );
                sample_res.setKBestPointsIndices ( k_best_points );

                //Set pointer to a sample
                Sample <T> *p_sample = &sample_res;

                //2D Helmert transformation: same results for rotated and unrotated sample
                if ( analysis_parameters.analysis_type.a_helt )
                        analyzeSampleHelmertTransformationDeviation ( *p_sample, *p_nl_test_best, *p_nl_projected_best, analysis_parameters.match_method, mult_ratio );

                //Create list for rotated test points and rotated sample as the copy of the result sample
                Container <Node3DCartesian <T> *> nl_test_best_rot;
                Sample <T> sample_rot ( sample_res );

                //Process unrotated and rotated sample
                bool rotated_sample = false;

                for ( unsigned int j = 0; j < 2 ; j++ )
                {
                        //2D homothetic transformation:
                        if ( analysis_parameters.analysis_type.a_homt )
                                analyzeSampleHomotheticTransformationDeviation ( *p_sample, *p_nl_test_best, *p_nl_projected_best, analysis_parameters.match_method, mult_ratio );

                        //Cross nearest distance
                        if ( analysis_parameters.analysis_type.a_cnd )
                                analyzeSampleCrossNearestNeighbourDistance ( *p_sample, *p_nl_test_best, *p_nl_projected_best, mult_ratio );

                        //Analysis of graticule: turning function
                        if ( analysis_parameters.analysis_type.a_gn_tf )
                                analyzeSampleGeographicNetworkTurningFunctionRatio ( *p_sample, *p_nl_test_best, *p_nl_projected_best, *p_meridians_best, *p_parallels_best, mult_ratio );

                        //Analysis of Voronoi diagrams: turning function
                        if ( analysis_parameters.analysis_type.a_vd_tf )
                                analyzeSampleUsingVoronoiDiagramTurningFunctionRatio ( *p_sample, *p_nl_test_best, *p_nl_projected_best, faces_test, analysis_parameters, mult_ratio );

                        //Angle of rotation given by 2D Helmert transformation
                        const T rot_angle = p_sample->getRotation();

                        //Is there a significant improvement for rotated sample: homt / helt > IMPROVE_RATIO_STD_DEV
                        //Is the angle of rotation in  interval (88, 92) + k*M_PI/2
                        rotated_sample = ( analysis_parameters.correct_rotation ) && ( IMPROVE_RATIO_STD_DEV * sample_res.getHelmertTransformationRatio() < sample_res.getHomotheticTransformationRatio() ) &&
                                         ( ( short ) ( fabs ( rot_angle ) + REM_DIV_ROT_ANGLE ) % 90 < 2 * REM_DIV_ROT_ANGLE ) && ( fabs ( rot_angle ) > MAX_LAT - REM_DIV_ROT_ANGLE );

                        //Rotate a sample by the -( angle )
                        if ( ( rotated_sample ) && ( !p_sample->getRotatedSample() ) )
                        {
                                //Create copy of test points
                                nl_test_best_rot = *p_nl_test_best;

                                //Rotate test points by angle given by 2D Helmert transformation in interval (80,100) + 0.5*k*M_PI
                                for ( unsigned int k = 0; k < n_best ; k++ )
                                {
                                        nl_test_best_rot[k]->setX ( ( *p_nl_test_best ) [k]->getX() * cos ( rot_angle * M_PI / 180.0 ) - ( *p_nl_test_best ) [k]->getY() * sin ( rot_angle * M_PI / 180.0 ) );
                                        nl_test_best_rot[k]->setY ( ( *p_nl_test_best ) [k]->getX() * sin ( rot_angle * M_PI / 180.0 ) + ( *p_nl_test_best ) [k]->getY() * cos ( rot_angle * M_PI / 180.0 ) );
                                }

                                //Set pointer to rotated sample
                                p_sample = &sample_rot;
                                p_nl_test_best = &nl_test_best_rot;

                                //Set angle and rotation flag for a sample
                                p_sample->setRotatedSample ( true );
                        }

                        else break;
                }

                //Compute sample cost
                sample_cost = ( rotated_sample ? sample_rot.getSampleCost ( analysis_parameters.analysis_type ) : sample_res.getSampleCost ( analysis_parameters.analysis_type ) );

                //Increment successfully created samples for projection
                created_samples ++;

                //Add also rotated sample to the list
                if ( rotated_sample )
                {
                        sample_res = sample_rot;
                        created_samples ++;
                }
        }

        //Return cost of the sample
        return sample_cost;
}


template <typename T>
bool CartAnalysis::checkSample ( const typename TMeridiansList <T> ::Type &meridians, const typename TParallelsList <T> ::Type &parallels, const Container <Node3DCartesian <T> *> &nl_test, const Container <Node3DCartesianProjected <T> *> &nl_projected, const T heuristic_sensitivity_ratio )
{
        //Small heuristic for sample: compare shape or the prime meridian, equator (central meridian, central parallel) north pole and south pole for test and reference points
        //using turning function und analyze Helmert transformation (fast match using circles)
        bool prime_meridian_found = false, equator_found = false;

        //Create list of transformed points
        Container <Node3DCartesian <T> *> nl_transformed;

        //Analyze match ratio using Helmert transformation ( additional test if no meridian and parallel have been found )
        TTransformationKeyHelmert2D <T> key_helmert;
        HelmertTransformation2D::transformPoints ( nl_projected, nl_test, nl_transformed, key_helmert );

        //Sample is too rotated
        const T rot_angle = atan2 ( key_helmert.c2, key_helmert.c1 ) * 180.0 / M_PI ;

        if ( ( short ) ( fabs ( rot_angle ) + 3.0 * REM_DIV_ROT_ANGLE ) % 90  > 6.0 * REM_DIV_ROT_ANGLE )
        {
                return false;
        }

        //Test match ratio: at least 75 percent of points matched (fast test with the circle)
        TIndexList matched_points;

        if ( Transformation2D::getMatchRatioCircle ( nl_projected, nl_transformed, matched_points, CollectOff, MATCHING_FACTOR * heuristic_sensitivity_ratio )  < 75 )
        {
                return false;
        }

        //Process all meridians and find prime meridian
        for ( typename TMeridiansList <T> ::Type::const_iterator i_meridians = meridians.begin(); i_meridians != meridians.end(); ++ i_meridians )
        {
                //Find prime meridian
                if ( ( *i_meridians ).getLon() == 0 )
                {
                        //Convert test meridian to Points 2D list
                        Container <Point3DCartesian <T> > pl_meridian_test ( nl_test, ( * i_meridians ).getPointsIndices () );

                        //Convert projected meridian to Points2D list
                        Container <Point3DCartesian <T> > pl_meridian_projected ( nl_projected, ( * i_meridians ).getPointsIndices () );

                        //Compute turning function difference for each test and projected meridian
                        T turning_function_ratio_prime_meridian = TurningFunction::compare2PolyLinesUsingTurningFunction ( pl_meridian_test, pl_meridian_projected, RotationInvariant, ScaleInvariant );

                        //Both prime meridians are not similar
                        if ( turning_function_ratio_prime_meridian > TURNING_FUNCTION_MAX_DIFFERENCE * pl_meridian_projected.size() * heuristic_sensitivity_ratio )
                        {
                                return false;
                        }

                        //Set prime meridian as found
                        prime_meridian_found = true;
                }
        }

        //Process all parallels find equator, north pole, south pole
        for ( typename TParallelsList <T> ::Type::const_iterator i_parallels = parallels.begin(); i_parallels != parallels.end(); ++ i_parallels )
        {
                //Find equator
                if ( ( *i_parallels ).getLat() == 0 )
                {
                        //Convert test equator to Points2D list
                        Container <Point3DCartesian <T> > pl_parallel_test ( nl_test, ( * i_parallels ).getPointsIndices () );

                        //Convert projected equator to Points2D list
                        Container <Point3DCartesian <T> > pl_parallel_projected ( nl_projected, ( * i_parallels ).getPointsIndices () );

                        //Compute turning function difference for each parallel
                        T turning_function_ratio_equator = TurningFunction::compare2PolyLinesUsingTurningFunction ( pl_parallel_test, pl_parallel_projected, RotationInvariant, ScaleInvariant );

                        //Both equators are not similar
                        if ( turning_function_ratio_equator > TURNING_FUNCTION_MAX_DIFFERENCE * pl_parallel_projected.size() * heuristic_sensitivity_ratio )
                        {
                                return false;
                        }

                        //Set equator as found
                        equator_found = true;
                }

                //Find and analyze north pole
                if ( ( *i_parallels ).getLat() == MAX_LAT )
                {
                        //Convert test parallel to Points2D list
                        Container <Point3DCartesian <T> > pl_parallel_test ( nl_test, ( * i_parallels ).getPointsIndices () );

                        //Convert projected parallel to Points2D list
                        Container <Point3DCartesian <T> > pl_parallel_projected ( nl_projected, ( * i_parallels ).getPointsIndices () );

                        //Compute turning function difference for each parallel
                        T turning_function_ratio_north_pole = TurningFunction::compare2PolyLinesUsingTurningFunction ( pl_parallel_test, pl_parallel_projected, RotationInvariant, ScaleInvariant );

                        //Both north poles are not similar
                        if ( turning_function_ratio_north_pole > TURNING_FUNCTION_MAX_DIFFERENCE * pl_parallel_projected.size() * heuristic_sensitivity_ratio )
                        {
                                return false;
                        }
                }

                //Find and analyze south pole
                if ( ( *i_parallels ).getLat() == MIN_LAT )
                {
                        //Convert test parallel to Points2D list
                        Container <Point3DCartesian <T> > pl_parallel_test ( nl_test, ( * i_parallels ).getPointsIndices () );

                        //Convert projected parallel to Points2D list
                        Container <Point3DCartesian <T> > pl_parallel_projected ( nl_projected, ( * i_parallels ).getPointsIndices () );

                        //Compute turning function difference for each parallel
                        T turning_function_ratio_south_pole = TurningFunction::compare2PolyLinesUsingTurningFunction ( pl_parallel_test, pl_parallel_projected, RotationInvariant, ScaleInvariant );

                        //Both south poles are not similar
                        if ( turning_function_ratio_south_pole > TURNING_FUNCTION_MAX_DIFFERENCE * pl_parallel_projected.size() * heuristic_sensitivity_ratio )
                        {
                                return false;
                        }
                }
        }

        //If not prime meridian found, continue with the found central meridian of the analyzed area
        if ( ( !prime_meridian_found ) && ( meridians.size() > 0 ) )
        {
                //Set central meridian of the dataset
                typename TMeridiansList <T> ::Type::const_iterator i_meridians = meridians.begin();

                for ( unsigned int i = 0; i < meridians.size() / 2; ++ i_meridians, ++ i ) {}

                //Convert test meridian to Points 2D list
                Container <Point3DCartesian <T> > pl_meridian_test ( nl_test, ( *i_meridians ).getPointsIndices () );

                //Convert projected meridian to Points2D list
                Container <Point3DCartesian <T> > pl_meridian_projected ( nl_projected, ( *i_meridians ).getPointsIndices () );

                //Compute turning function difference for each test and projected meridian
                T turning_function_ratio_meridian = TurningFunction::compare2PolyLinesUsingTurningFunction ( pl_meridian_test, pl_meridian_projected, RotationInvariant, ScaleInvariant );

                //Both prime meridians are not similar
                if ( turning_function_ratio_meridian > TURNING_FUNCTION_MAX_DIFFERENCE * pl_meridian_projected.size() * heuristic_sensitivity_ratio )
                {
                        return false;
                }
        }

        //If not equator found, continue with the found central parallel of the analyzed area
        if ( ( !equator_found ) && ( parallels.size() > 0 ) )
        {
                //Set central parallel of the dataset
                typename TParallelsList <T> ::Type::const_iterator i_parallels = parallels.begin();

                for ( unsigned int i = 0; i < parallels.size() / 2; ++ i_parallels, ++ i ) {}

                //Convert test parallel to Points2D list
                Container <Point3DCartesian <T> > pl_parallel_test ( nl_test, ( *i_parallels ).getPointsIndices () );

                //Convert projected parallel to Points2D list
                Container <Point3DCartesian <T> > pl_parallel_projected ( nl_projected, ( *i_parallels ).getPointsIndices () );

                //Compute turning function difference for each parallel
                T turning_function_ratio_parallel = TurningFunction::compare2PolyLinesUsingTurningFunction ( pl_parallel_test, pl_parallel_projected, RotationInvariant, ScaleInvariant );

                //Both equators are not similar
                if ( turning_function_ratio_parallel >  TURNING_FUNCTION_MAX_DIFFERENCE * pl_parallel_projected.size() * heuristic_sensitivity_ratio )
                {
                        return false;
                }
        }

        //All test successfull, sample may be perspective
        return true;
}


template <typename T>
void CartAnalysis::redLon ( const Container <Point3DGeographic <T> *> & pl_source, const T lon0, Container <Point3DGeographic <T> *> & pl_destination )
{
        //Corect lon0: set new central meridian for all items of the destination container
        for ( unsigned int i = 0; i < pl_source.size(); i++ )
        {
                //Create copy of a point
                Point3DGeographic <T> *point = pl_source [i] ->clone();

                //Set new longitude
                point->setLon ( CartTransformation::redLon0 ( point->getLon(), lon0 ) );

                //Add point with modified longitude to the list
                pl_destination.push_back ( point );
        }
}


template <typename T>
void CartAnalysis::redLon ( const Container <Point3DGeographic <T> *> & pl_source, const T lon0 )
{
        //Corect lon0: set new central meridian for all items of the destination container
        for ( unsigned int i = 0; i < pl_source.size(); i++ )
        {
                //Create copy of a point
                pl_source [i]->setLon ( CartTransformation::redLon0 ( pl_source [i]->getLon(), lon0 ) );
        }
}


template <typename T>
void CartAnalysis::removeSingularPoints ( const Container <Node3DCartesian <T> *> & nl_source, const Container <Point3DGeographic <T> *> & pl_source, const Projection <T>  *proj, Container <Node3DCartesian <T> *> &nl_destination, Container <Point3DGeographic <T> *> &pl_destination, typename TDevIndexPairs <T>::Type & non_singular_point_pairs )
{
        //Remove all singular points from computations
        for ( unsigned int i = 0; i < nl_source.size(); i++ )
        {
                if ( ( * pl_source [i] != proj->getCartPole() ) && ( ( proj->getCartPole().getLon() >= 0 ) && ( pl_source [i]->getLon() != proj->getCartPole().getLon() - 180.0 ) ||
                                ( proj->getCartPole().getLon() < 0 ) && ( pl_source [i]->getLon() != proj->getCartPole().getLon() + 180.0 ) ) )
                {
                        nl_destination.push_back ( nl_source [i]->clone() );
                        pl_destination.push_back ( pl_source [i]->clone() );

                        non_singular_point_pairs.push_back ( std::make_pair ( i + 1.0, i ) );
                }
        }
}


template <typename T>
void CartAnalysis::removeSingularPoints ( const Container <Node3DCartesian <T> *> &nl_source, const Container <Point3DGeographic <T> *> &pl_source, const Projection <T>  *proj, typename TDevIndexPairs <T>::Type & non_singular_point_pairs )
{
        //Remove all singular points from computations
        typename TItemsList <Node3DCartesian <T> *> ::Type ::iterator i_nl = nl_source.begin();
        typename TItemsList <Point3DGeographic <T> *> ::Type ::iterator i_pl = pl_source.begin();

        for ( unsigned int i = 0; ( i_nl != nl_source.end() ) && ( i_pl != pl_source.end() ); i++ )
        {
                if ( ( * pl_source [i] != proj->getCartPole() ) && ( ( proj->getCartPole().getLon() >= 0 ) && ( pl_source [i]->getLon() != proj->getCartPole().getLon() - 180.0 ) ||
                                ( proj->getCartPole().getLon() < 0 ) && ( pl_source [i]->getLon() != proj->getCartPole().getLon() + 180.0 ) ) )
                {
                        nl_source->erase ( i_nl++ ); pl_source->erase ( i_pl++ );
                        non_singular_point_pairs.push_back ( std::make_pair ( i + 1.0, i ) );
                }
                else
                {
                        ++i_nl; ++i_pl;
                }
        }
}


template <typename T>
void CartAnalysis::correctMeridiansAndParrallels ( typename TMeridiansList <T> ::Type & meridians, typename TParallelsList <T> ::Type & parallels,
                typename TDevIndexPairs<T>::Type & point_pairs )
{
        //Process meridians and paralles: remove inappropriate points detected as outliers using IRLS

        //Process all meridians
        for ( typename TMeridiansList <T> ::Type::iterator i_meridians = meridians.begin(); i_meridians != meridians.end(); )
        {
                //Get points of the meridian: break invariant
                TIndexList &meridian_points = const_cast <TIndexList&> ( ( *i_meridians ).getPointsIndices() );

                //Compare meridian points with list of pairs: remove indices of points missing in the list of pairs
                meridian_points.erase ( std::remove_if ( meridian_points.begin(), meridian_points.end(),
                                        removeUnequalMeridianParallelPointIndices <T> ( point_pairs ) ), meridian_points.end() );

                //Transform all indices of k-best points to new index list
                std::transform ( meridian_points.begin(), meridian_points.end(), meridian_points.begin(), findMeridianParallelPointIndices <T> ( point_pairs ) );

                //Not enough points, erase meridian from set
                if ( meridian_points.size() < RANSAC_MIN_LINE_POINTS )
                {
                        meridians.erase ( i_meridians++ );
                        continue;
                }
                else ++i_meridians;
        }

        //Process all parallels
        for ( typename TParallelsList <T> ::Type::iterator i_parallels = parallels.begin(); i_parallels != parallels.end(); )
        {
                //Get points of the parallel: break invariant
                TIndexList &parallel_points  = const_cast <TIndexList&> ( ( *i_parallels ).getPointsIndices() );

                //Compare parallel points with list of pairs: remove indices of points missing in the list of pairs
                parallel_points.erase ( std::remove_if ( parallel_points.begin(), parallel_points.end(),
                                        removeUnequalMeridianParallelPointIndices <T> ( point_pairs ) ), parallel_points.end() );

                //Transform all indices of k-best points to new index list
                std::transform ( parallel_points.begin(), parallel_points.end(), parallel_points.begin(), findMeridianParallelPointIndices <T> ( point_pairs ) );

                //Not enough points, erase parallel from set
                if ( parallel_points.size() < RANSAC_MIN_LINE_POINTS )
                {
                        parallels.erase ( i_parallels ++ );
                        continue;
                }
                else ++i_parallels;
        }
}



template <typename T>
void CartAnalysis::correctPointsMeridiansAndParrallels ( const Container <Node3DCartesian <T> *> &nl_test_corr, const Container <Point3DGeographic <T> *> &pl_reference_corr, const typename TMeridiansList <T> ::Type & meridians, const typename TParallelsList <T> ::Type & parallels, const unsigned int n,
                Container <Node3DCartesian <T> *> **p_nl_test, Container <Point3DGeographic <T> *> **p_pl_reference, typename TMeridiansList <T> ::Type ** p_meridians, typename TParallelsList <T> ::Type ** p_parallels, typename TMeridiansList <T> ::Type & meridians_corr,
                typename TParallelsList <T> ::Type & parallels_corr, typename TDevIndexPairs<T>::Type & point_pairs_corr, bool &uncorrect_points_found )
{
        //Process points, meridians and paralles: remove inappropriate points, correct meridians and parallels
        const unsigned int n_corr = nl_test_corr.size();

        //Correction of meridians, parallels and points must be done
        if ( n != n_corr )
        {
                //Set flag to true, used for all samples using non-singular sets
                uncorrect_points_found = true;

                //Create copy of meridians / parallels
                meridians_corr = meridians; parallels_corr = parallels;

                //Process all meridians
                for ( typename TMeridiansList <T> ::Type::iterator i_meridians = meridians.begin(); i_meridians != meridians.end(); )
                {
                        //Get points of the meridian: break invariant
                        TIndexList &meridian_points = const_cast <TIndexList&> ( i_meridians -> getPointsIndices() );

                        //Compare meridian points with list of pairs: remove indices of points missing in the list of pairs
                        meridian_points.erase ( std::remove_if ( meridian_points.begin(), meridian_points.end(),
                                                removeUnequalMeridianParallelPointIndices <T> ( point_pairs_corr ) ), meridian_points.end() );

                        //Transform all indices of k-best points to new index list
                        std::transform ( meridian_points.begin(), meridian_points.end(), meridian_points.begin(), findMeridianParallelPointIndices <T> ( point_pairs_corr ) );

                        //Not enough points, erase meridian from set
                        if ( meridian_points.size() < RANSAC_MIN_LINE_POINTS )
                        {
                                meridians.erase ( i_meridians++ );
                                continue;
                        }
                        else ++i_meridians;
                }

                //Process all parallels
                for ( typename TParallelsList <T> ::Type::iterator i_parallels = parallels.begin(); i_parallels != parallels.end(); )
                {
                        //Get points of the parallel: break invariant
                        TIndexList &parallel_points  = const_cast <TIndexList&> ( i_parallels ->getPointsIndices() );

                        //Compare parallel points with list of pairs: remove indices of points missing in the list of pairs
                        parallel_points.erase ( std::remove_if ( parallel_points.begin(), parallel_points.end(),
                                                removeUnequalMeridianParallelPointIndices <T> ( point_pairs_corr ) ), parallel_points.end() );

                        //Transform all indices of k-best points to new index list
                        std::transform ( parallel_points.begin(), parallel_points.end(), parallel_points.begin(), findMeridianParallelPointIndices <T> ( point_pairs_corr ) );

                        //Not enough points, erase parallel from set
                        if ( parallel_points.size() < RANSAC_MIN_LINE_POINTS )
                        {
                                parallels.erase ( i_parallels ++ );
                                continue;
                        }
                        else ++i_parallels;
                }

                //Convert new pairs to index list: indices will be printed to the log file
                std::transform ( point_pairs_corr.begin(), point_pairs_corr.end(), std::back_inserter ( point_pairs_corr ), getSecondElementInPair() );

                //Set pointers to corrected meridians and parallels
                p_meridians = &meridians_corr; p_parallels = &parallels_corr;

                //Set pointers to corrected sets
                p_nl_test = &nl_test_corr; p_pl_reference = &pl_reference_corr;
        }

        //No correction neccessary
        else
        {
                //Set flag to true, used for all samples using non-singular sets
                uncorrect_points_found = false;

                //Initialize point indices: all indices are valid
                for ( unsigned int j = 0; j < n_corr; j++ ) point_pairs_corr.push_back ( j );
        }

}


//Each sample analysis
template <typename T>
void CartAnalysis::analyzeSampleCrossNearestNeighbourDistance ( Sample <T> &s, const Container <Node3DCartesian <T> *> &nl_test,  const Container <Node3DCartesianProjected  <T> *> &nl_projected, const float mult_ratio )
{
        //Analyze all samples using cross nearest distance ratio
        try
        {
                //List of transformed points
                Container <Node3DCartesian <T> *> nl_transformed;

                //Compute homothetic transformation
                TTransformationKeyHomothetic2D <T> key_homothetic;
                HomotheticTransformation2D::transformPoints ( nl_projected, nl_test, nl_transformed, key_homothetic );

                //Compute cross nearest distance ratio
                T cross_nearest_neighbour_distance_ratio = NNDistance::getCrossNearestNeighbourDistance ( nl_projected, nl_transformed );

                //Set ratio for the sample
                s.setCrossNearestNeighbourDistanceRatio ( mult_ratio * cross_nearest_neighbour_distance_ratio );
                s.setCrossNearestNeighbourDistanceRatioPosition ( 1 );
        }

        //Throw exception
        catch ( Error & error )
        {
                s.setCrossNearestNeighbourDistanceRatio ( MAX_FLOAT );
                s.setCrossNearestNeighbourDistanceRatioPosition ( -1 );
        }
}



template <typename T>
void CartAnalysis::analyzeSampleHomotheticTransformationDeviation ( Sample <T> &s, const Container <Node3DCartesian <T> *> &nl_test,  const Container <Node3DCartesianProjected  <T> *> &nl_projected, const TMatchPointsType match_type, const float mult_ratio )
{
        //Analyze sample using Homothetic transformation deviation
        try
        {
                //List of transformed points
                Container <Node3DCartesian <T> *> nl_transformed;

                //Compute homothetic transformation
                TTransformationKeyHomothetic2D <T> key_homothetic;
                HomotheticTransformation2D::transformPoints ( nl_projected, nl_test, nl_transformed, key_homothetic );

                //Compute ratio and percentage match using uncertainty regions
                TIndexList matched_points;
                TAccuracyCharacteristics <T> deviations = Transformation2D::getAccuracyCharacteristics ( nl_projected, nl_test, nl_transformed, key_homothetic );
                T homothetic_transformation_ratio = deviations.std_dev;
                T homothetic_transformation_perc_match = ( match_type == MatchCircle ? Transformation2D::getMatchRatioCircle ( nl_projected, nl_transformed, matched_points, CollectOn, 0.1 ) :
                                Transformation2D::getMatchRatioTissotIndikatrix ( nl_projected, nl_transformed, matched_points, CollectOn, 0.5 ) );

                //Set ratio, percentage match, scale, tx and ty for the sample
                s.setHomotheticTransformationRatio ( mult_ratio * homothetic_transformation_ratio );
                s.setHomotheticTransformationRatioPosition ( 1 );
                s.setHomotheticTransformationPercMatch ( ( unsigned int ) homothetic_transformation_perc_match );
                s.setHomotheticTransformationMatchedPointsIndices ( matched_points );
                s.setScaleHomT ( key_homothetic.c );
                s.setDx ( key_homothetic.x_mass_local - key_homothetic.x_mass_global / key_homothetic.c );
                s.setDy ( key_homothetic.y_mass_local - key_homothetic.y_mass_global / key_homothetic.c );
        }

        //Throw exception
        catch ( Error & error )
        {
                s.setHomotheticTransformationRatio ( MAX_FLOAT );
                s.setHomotheticTransformationPercMatch ( 0.0 );
                s.setHomotheticTransformationRatioPosition ( -1 );
        }
}


template <typename T>
void CartAnalysis::analyzeSampleHelmertTransformationDeviation ( Sample <T> &s, const Container <Node3DCartesian <T> *> &nl_test,  const Container <Node3DCartesianProjected  <T> *> &nl_projected, const TMatchPointsType match_type, const float mult_ratio )
{
        //Analyze sample using Helmert transformation deviation
        try
        {
                //List of transformed points
                Container <Node3DCartesian <T> *> nl_transformed;

                //Compute Helmert transformation
                TTransformationKeyHelmert2D <T> key_helmert;
                HelmertTransformation2D::transformPoints ( nl_projected, nl_test, nl_transformed, key_helmert );

                //Compute ratio and percentage match using uncertainty regions
                TIndexList matched_points;
                TAccuracyCharacteristics <T> deviations = Transformation2D::getAccuracyCharacteristics ( nl_projected, nl_test, nl_transformed, key_helmert );
                T helmert_transformation_ratio = deviations.std_dev;
                T helmert_transformation_perc_match = ( match_type == MatchCircle  ? Transformation2D::getMatchRatioCircle ( nl_projected, nl_transformed, matched_points, CollectOn, 0.1 ) :
                                                        Transformation2D::getMatchRatioTissotIndikatrix ( nl_projected, nl_transformed, matched_points, CollectOn, 0.5 ) );

                //Set ratio and percentage match for the sample
                s.setHelmertTransformationRatio ( mult_ratio * helmert_transformation_ratio );
                s.setHelmertTransformationRatioPosition ( 1 );
                s.setHelmertTransformationPercMatch ( ( unsigned int ) helmert_transformation_perc_match );
                s.setHelmertTransformationMatchedPointsIndices ( matched_points );
                s.setScaleHelT ( sqrt ( key_helmert.c1 * key_helmert.c1 + key_helmert.c2 * key_helmert.c2 ) );
                s.setRotation ( atan2 ( key_helmert.c2, key_helmert.c1 ) * 180.0 / M_PI );
                s.setDx ( key_helmert.x_mass_local - key_helmert.x_mass_global / sqrt ( key_helmert.c1 * key_helmert.c1 + key_helmert.c2 * key_helmert.c2 ) );
                s.setDy ( key_helmert.y_mass_local - key_helmert.y_mass_global / sqrt ( key_helmert.c1 * key_helmert.c1 + key_helmert.c2 * key_helmert.c2 ) );
        }

        //Throw exception
        catch ( Error & error )
        {
                s.setHelmertTransformationRatio ( MAX_FLOAT );
                s.setHelmertTransformationPercMatch ( 0.0 );
                s.setHelmertTransformationRatioPosition ( -1 );
        }
}


template <typename T>
void CartAnalysis::analyzeSampleGeographicNetworkTurningFunctionRatio ( Sample <T> &s, const Container <Node3DCartesian <T> *> &nl_test,  const Container <Node3DCartesianProjected  <T> *> &nl_projected,
                const typename TMeridiansList <T> ::Type &meridians, const typename TParallelsList <T> ::Type &parallels, const float mult_ratio )
{
        //Analyze sample using angular turning function differences in geographic network
        T turning_function_ratio_meridians = 0, turning_function_ratio_parallels = 0;

        try
        {
                //No meridian or parallel
                if ( ( meridians.size() == 0 ) && ( parallels.size() == 0 ) )
                {
                        throw ErrorBadData ( "ErrorBadData: no meridians and parallels. ", "Can not perform analysis of turning function." );
                }

                typename TMeridiansList <T> ::Type::const_iterator i_meridians = meridians.begin();

                //Process all meridians
                for ( i_meridians = meridians.begin(); i_meridians != meridians.end(); ++i_meridians )
                {
                        TIndexList il = ( * i_meridians ).getPointsIndices () ;

                        //Convert test meridian to Points 2D list
                        Container <Point3DCartesian <T> > pl_meridian_test ( nl_test, ( * i_meridians ).getPointsIndices () );

                        //Convert projected meridian to Points2D list
                        Container <Point3DCartesian <T> > pl_meridian_projected ( nl_projected, ( * i_meridians ).getPointsIndices () );

                        //Compute turning function difference for each test and projected meridian
                        turning_function_ratio_meridians += TurningFunction::compare2PolyLinesUsingTurningFunction ( pl_meridian_test, pl_meridian_projected, RotationDependent, ScaleInvariant );
                }

                typename TParallelsList <T> ::Type::const_iterator i_parallels = parallels.begin();

                //Process all parallels
                for ( i_parallels = parallels.begin(); i_parallels != parallels.end(); ++i_parallels )
                {
                        //Convert test meridian to Points2D list
                        Container <Point3DCartesian <T> > pl_parallel_test ( nl_test, ( * i_parallels ).getPointsIndices () );

                        //Convert projected meridian to Points2D list
                        Container <Point3DCartesian <T> > pl_parallel_projected ( nl_projected, ( * i_parallels ).getPointsIndices () );

                        //Compute turning function difference for each parallel
                        turning_function_ratio_parallels += TurningFunction::compare2PolyLinesUsingTurningFunction ( pl_parallel_test, pl_parallel_projected, RotationDependent, ScaleInvariant );
                }

                //Set turning function ratio
                s.setGNTurningFunctionRatio ( mult_ratio * ( turning_function_ratio_meridians  +  turning_function_ratio_parallels ) );
                s.setGNTurningFunctionRatioPosition ( 1 );
        }

        //Throw exception
        catch ( Error & error )
        {
                s.setGNTurningFunctionRatio ( MAX_FLOAT );
                s.setGNTurningFunctionRatioPosition ( -1 );
        }
}



template <typename T, TDestructable destructable>
void CartAnalysis::analyzeSampleUsingVoronoiDiagramTurningFunctionRatio ( Sample <T> &s, const Container <Node3DCartesian <T> *> &nl_test, const Container <Node3DCartesianProjected  <T> *> &nl_projected, const Container <Face <T> *, destructable> &faces_test,
                const TAnalysisParameters <T> & analysis_parameters, const float mult_ratio )
{
        //Analyze sample using Voronoi diagram ratios
        try
        {
                const unsigned int n_test_points = nl_test.size();

                //Compute Voronoi diagram for test dataset after outliers removed
                Container <HalfEdge <double> *> hl_dt_test, hl_vor_test, hl_merge_test;
                Container <Node3DCartesian <double> *> nl_vor_test, intersections_test;
                Container <VoronoiCell <double> *> vor_cells_test;

                //Voronoi diagram of the reference dataset, data structures
                Container <HalfEdge <T> *> hl_dt_reference, hl_vor_reference, hl_merge_reference;
                Container <Node3DCartesian <T> *> nl_vor_reference, intersections_reference;
                Container <VoronoiCell <T> *> vor_cells_list_reference;

                //Create Voronoi diagram for the reference dataset in any case
                Voronoi2D::VD ( ( Container <Node3DCartesian <T> *> & ) nl_projected, nl_vor_reference, hl_dt_reference, hl_vor_reference, vor_cells_list_reference, AppropriateBoundedCells, TopologicApproach, 0, analysis_parameters.print_exceptions );

                //Modified dataset with found outliers, rotated dataset or dataset with found singular points: create new Voronoi diagram for the test dataset
                //Do not use pre-generated test faces
                if ( s.getOutliersFound() || s.getRotatedSample() || s.getSingularPointsFound() )
                        Voronoi2D::VD ( ( Container <Node3DCartesian <double> *> & ) nl_test, nl_vor_test, hl_dt_test, hl_vor_test, vor_cells_test, AppropriateBoundedCells, TopologicApproach, 0, analysis_parameters.print_exceptions );

                //Get total unbounded cells for the test dataset
                unsigned int total_bounded_pairs_of_cell = 0;

                //Initialize ratio
                T turning_function_difference = 0.0;

                //Process all merged faces
                for ( unsigned int index_faces = 0; index_faces < n_test_points; index_faces++ )
                {
                        //An appropriate face in the non-modified test dataset exists
                        //For modified  datasets with found outliers, rotated or found singularitities: Construct a new face for test dataset
                        if ( ( s.getOutliersFound() || s.getRotatedSample() || s.getSingularPointsFound() ) || faces_test [index_faces] != NULL )
                        {
                                //Get Voronoi cell of the reference dataset
                                VoronoiCell <T> *vor_cell_reference = dynamic_cast < VoronoiCell <T> * > ( nl_projected [index_faces] -> getFace() );

                                //Get Voronoi cells of the modified test dataset from newly created Voronoi diagram: only if outliers found, rotated dataset or singular points found
                                VoronoiCell <T> *vor_cell_test = ( s.getOutliersFound() || s.getRotatedSample() || s.getOutliersFound() ? dynamic_cast < VoronoiCell <T> * > ( nl_test [index_faces] -> getFace() ) : NULL );

                                //Remove_poutliers = 0, rotated dataset = 0 and remove singular points = 0: bounded Voronoi cell of the reference dataset exists and corresponding face of the test dataset exists
                                //Remove_poutliers = 1, rotated dataset = 1 and remove singulat points = 1: bounded Voronoi cell of the reference datasets exist and corresponding bounded Voronoi cell exist
                                if ( ( vor_cell_reference != NULL ) && ( vor_cell_reference->getBounded() ) && ( ( !s.getOutliersFound() && !s.getRotatedSample() && !s.getSingularPointsFound () ) ||
                                                ( vor_cell_test != NULL ) && ( vor_cell_test->getBounded() ) && ( s.getOutliersFound() || s.getRotatedSample() || s.getSingularPointsFound() ) ) )
                                {
                                        //Pointer to merged reference face
                                        Face <T> * face_reference = NULL, *face_test = NULL;

                                        try
                                        {
                                                //Merge reference Voronoi cell with adjacent cells
                                                Voronoi2D::mergeVoronoiCellAndAdjacentCells ( vor_cell_reference, &face_reference, intersections_reference, hl_merge_reference );

                                                //Merge test Voronoi cell with adjacent cells: only if outliers found, dataset is rotated or singular points found
                                                if ( s.getOutliersFound() || s.getRotatedSample() || s.getSingularPointsFound() )
                                                        Voronoi2D::mergeVoronoiCellAndAdjacentCells ( vor_cell_test, &face_test, intersections_test, hl_merge_test );

                                                //Count unbounded pairs of Voronoi cells
                                                total_bounded_pairs_of_cell ++;

                                                //Compute turning function difference for both merged faces: for modified dataset use newly created face, otherwise use a precomputed face
                                                turning_function_difference += s.getOutliersFound() || s.getRotatedSample() || s.getSingularPointsFound() ? TurningFunction::compare2FacesUsingTurningFunction ( face_test, face_reference, RotationDependent, ScaleInvariant ) :
                                                                               TurningFunction::compare2FacesUsingTurningFunction ( faces_test [index_faces], face_reference, RotationDependent, ScaleInvariant );

                                                //Delete merged face
                                                if ( face_reference != NULL ) delete face_reference;

                                                if ( face_test != NULL ) delete face_test;
                                        }

                                        //Throw exception
                                        catch ( Error & error )
                                        {
                                                if ( face_reference != NULL ) delete face_reference;

                                                if ( face_test != NULL ) delete face_test;

                                                throw error;
                                        }
                                }
                        }
                }

                //Not enough corresponding pairs of bounded Voronoi cells suitable for analysis
                if ( total_bounded_pairs_of_cell < MIN_BOUNDED_VORONOI_CELLS )
                {
                        throw ErrorBadData ( "ErrorBadData: not enough unbounded pairs, ", "set values" );
                }

                //Set results of the analysis
                s.setVoronoiCellTurningFunctionRatio ( mult_ratio * sqrt ( turning_function_difference / total_bounded_pairs_of_cell ) );
                s.setVoronoiCellTurningFunctionRatioPosition ( 1 );
        }

        //Error while sample processed or invalid configuration for cell by cell ratia
        catch ( Error & error )
        {
                //Do not compute other analysis
                if ( analysis_parameters.analysis_type.a_vd_tf )
                {
                        s.setVoronoiCellTurningFunctionRatio ( MAX_FLOAT );
                        s.setVoronoiCellTurningFunctionRatioPosition ( -1 );
                }
        }
}



template <typename T>
void CartAnalysis::sortSamplesByComputedRatios ( Container <Sample <T> > &sl, const typename TAnalysisParameters<T>::TAnalysisType & analysis_type )
{

        //Sort results: Cross nearest distance
        if ( analysis_type.a_cnd )
                std::sort ( sl.begin(), sl.end(), sortSamplesByCrossNearestNeighbourDistanceRatio () );

        typename TAnalysisParameters <T>::TAnalysisType a1 ( analysis_type.a_cnd, 0, 0, 0, 0 ) ;
        setPositionForSortedSamples ( sl, a1 );

        //Sort results: Homothetic transformation, standard deviation + match function
        if ( analysis_type.a_homt )
                std::sort ( sl.begin(), sl.end(), sortSamplesByHomotheticTransformationRatio () );

        typename TAnalysisParameters <T>::TAnalysisType a2 ( 0, analysis_type.a_homt, 0, 0, 0 );
        setPositionForSortedSamples ( sl, a2 );

        //Sort results: Helmert transformation, standard deviation + match function
        if ( analysis_type.a_helt )
                std::sort ( sl.begin(), sl.end(), sortSamplesByHelmertTransformationRatio () );

        typename TAnalysisParameters <T>::TAnalysisType a3 ( 0, 0, analysis_type.a_helt, 0, 0 );
        setPositionForSortedSamples ( sl, a3 );

        //Sort results: geographic network turning function difference ratio
        if ( analysis_type.a_gn_tf )
                std::sort ( sl.begin(), sl.end(), sortSamplesByGNTurningFunctionRatio() );

        typename TAnalysisParameters <T>::TAnalysisType a4 ( 0, 0, 0, analysis_type.a_gn_tf, 0 );
        setPositionForSortedSamples ( sl, a4 );

        //Sort results: Voronoi cell Turning Function Ratio
        if ( analysis_type.a_vd_tf )
                std::sort ( sl.begin(), sl.end(), sortSamplesByVoronoiCellTurningFunctionRatio() );

        typename TAnalysisParameters <T>::TAnalysisType a5 ( 0, 0, 0, 0, analysis_type.a_vd_tf );
        setPositionForSortedSamples ( sl, a5 );

        //Sort samples by all ratios: result = arithmetic mean
        std::sort ( sl.begin(), sl.end(), sortSamplesByAllRatios <T> ( analysis_type ) );
}


template <typename T>
void CartAnalysis::setPositionForSortedSamples ( Container <Sample <T> > &sl, const typename TAnalysisParameters<T>::TAnalysisType & analysis_type )
{
        //Set position for each sample after its sorting
        unsigned int n = sl.size();

        for ( unsigned int i = 1; i < n; i++ )
        {
                //Cross nearest distance criterion
                if ( analysis_type.a_cnd )
                {
                        //Actual value differs from previous value: their difference > min value
                        if ( fabs ( sl [i].getCrossNearestNeighbourDistanceRatio() -
                                        sl [i - 1].getCrossNearestNeighbourDistanceRatio() ) > ARGUMENT_ROUND_ERROR )
                        {
                                //Previous value negative: first position
                                if ( sl [i - 1].getCrossNearestNeighbourDistanceRatioPosition() < 0 )
                                        sl [i].setCrossNearestNeighbourDistanceRatioPosition ( 1 );

                                //Previous value positive: position = position + 1
                                else if ( sl [i - 1].getCrossNearestNeighbourDistanceRatioPosition() > 0 )
                                {
                                        if ( sl [i].getCrossNearestNeighbourDistanceRatioPosition() > 0 )
                                                sl [i].setCrossNearestNeighbourDistanceRatioPosition ( sl [i - 1].getCrossNearestNeighbourDistanceRatioPosition() + 1 );
                                }
                        }

                        //Actual value same as the previous value: both values are equal
                        else
                        {
                                sl [i].setCrossNearestNeighbourDistanceRatioPosition ( sl [i - 1].getCrossNearestNeighbourDistanceRatioPosition() );
                        }
                }

                //Homothetic transformation, standard deviation criterion + match factor
                if ( analysis_type.a_homt )
                {
                        //Actual value differs from previous value: their difference > min value
                        if ( fabs ( sl [i].getHomotheticTransformationRatio() -
                                        sl [i - 1].getHomotheticTransformationRatio() ) > ARGUMENT_ROUND_ERROR )
                        {
                                //Previous value negative: first position
                                if ( sl [i - 1].getHomotheticTransformationRatioPosition() < 0 )
                                        sl [i].setHomotheticTransformationRatioPosition ( 1 );

                                //Previous value positive: position = position + 1
                                else if ( sl [i - 1].getHomotheticTransformationRatioPosition() > 0 )
                                {
                                        if ( sl [i].getHomotheticTransformationRatioPosition() > 0 )
                                                sl [i].setHomotheticTransformationRatioPosition ( sl [i - 1].getHomotheticTransformationRatioPosition() + 1 );
                                }
                        }

                        //Actual value same as the previous value: both values are equal
                        else
                        {
                                sl [i].setHomotheticTransformationRatioPosition ( sl [i - 1].getHomotheticTransformationRatioPosition() );
                        }
                }

                //Helmert transformation, standard deviation criterion + match factor
                if ( analysis_type.a_helt )
                {
                        //Actual value differs from previous value: their difference > min value
                        if ( fabs ( sl [i].getHelmertTransformationRatio() -
                                        sl [i - 1].getHelmertTransformationRatio() ) > ARGUMENT_ROUND_ERROR )
                        {
                                //Previous value negative: first position
                                if ( sl [i - 1].getHelmertTransformationRatioPosition() < 0 )
                                        sl [i].setHelmertTransformationRatioPosition ( 1 );

                                //Previous value positive: position = position + 1
                                else if ( sl [i - 1].getHelmertTransformationRatioPosition() > 0 )
                                {
                                        if ( sl [i].getHelmertTransformationRatioPosition() > 0 )
                                                sl [i].setHelmertTransformationRatioPosition ( sl [i - 1].getHelmertTransformationRatioPosition() + 1 );
                                }
                        }

                        //Actual value same as the previous value: both values are equal
                        else
                        {
                                sl [i].setHelmertTransformationRatioPosition ( sl [i - 1].getHelmertTransformationRatioPosition() );
                        }
                }

                //Turning function criterion for meridians and parallels
                else if ( analysis_type.a_gn_tf )
                {
                        //Actual value differs from previous value: their difference > min value
                        if ( fabs ( sl [i].getGNTurningFunctionRatio() -
                                        sl [i - 1].getGNTurningFunctionRatio() ) > ARGUMENT_ROUND_ERROR )
                        {
                                //Previous value negative: first position
                                if ( sl [i - 1].getGNTurningFunctionRatioPosition() < 0 )
                                        sl [i].setGNTurningFunctionRatioPosition ( 1 );

                                //Previous value positive: position = position + 1
                                else if ( sl [i - 1].getGNTurningFunctionRatioPosition() > 0 )
                                {
                                        if ( sl [i].getGNTurningFunctionRatioPosition() > 0 )
                                                sl [i].setGNTurningFunctionRatioPosition ( sl [i - 1].getGNTurningFunctionRatioPosition() + 1 );
                                }
                        }

                        //Actual value same as the previous value: both values are equal
                        else
                        {
                                sl [i].setGNTurningFunctionRatioPosition ( sl [i - 1].getGNTurningFunctionRatioPosition() );
                        }
                }

                //Voronoi cell turning function criterion
                else if ( analysis_type.a_vd_tf )
                {
                        //Actual value differs from previous value: their difference > min value
                        if ( fabs ( sl [i].getVoronoiCellTurningFunctionRatio() -
                                        sl [i - 1].getVoronoiCellTurningFunctionRatio() ) > ARGUMENT_ROUND_ERROR )
                        {
                                //Previous value negative: position = 1
                                if ( sl [i - 1].getVoronoiCellTurningFunctionRatioPosition() < 0 )
                                        sl [i].setVoronoiCellTurningFunctionRatioPosition ( 1 );

                                //Previous value positive: position = position + 1
                                else if ( sl [i - 1].getVoronoiCellTurningFunctionRatioPosition() > 0 )
                                {
                                        if ( sl [i].getVoronoiCellTurningFunctionRatioPosition() > 0 )
                                                sl [i].setVoronoiCellTurningFunctionRatioPosition ( sl [i - 1].getVoronoiCellTurningFunctionRatioPosition() + 1 );
                                }
                        }

                        //Actual value same as the previous value: both values are equal
                        else
                        {
                                sl [i].setVoronoiCellTurningFunctionRatioPosition ( sl [i - 1].getVoronoiCellTurningFunctionRatioPosition() );
                        }
                }
        }
}


template <typename T>
void CartAnalysis::printResults ( const Container <Sample <T> > &sl, const Container < Node3DCartesian <T> *> &nl_test, const Container <Point3DGeographic <T> *> &nl_reference,
                                  const TAnalysisParameters <T> & analysis_parameters, std::ostream * output )
{
        //Print first n items sorted by the similarity match ratio
        unsigned int items_printed = analysis_parameters.printed_results;
        const unsigned int n = sl.size(), n_test = nl_test.size();

        //Correct number of printed items
        if ( items_printed > n ) items_printed = n;

        //Some points were loaded
        if ( n > 0 )
        {
                //Table  1
                *output << "Results containg values of the criteria:" << std::endl << std::endl;

                //Set properties
                *output << std::showpoint << std::fixed << std::right;

                //Create header 1 : results of analalysis
                *output	<< std::setw ( 4 ) << "#"
                        << std::setw ( 8 ) << "Proj"
                        << std::setw ( 7 ) << "Categ"
                        << std::setw ( 6 ) << "latP"
                        << std::setw ( 7 ) << "lonP"
                        << std::setw ( 6 ) << "lat0"
                        << std::setw ( 7 ) << "lon0"
                        << std::setw ( 6 ) << "BKEY"
                        << std::setw ( 9 ) << "CND[m]"
                        << std::setw ( 9 ) << "HOMT[m]";

                //Print type of match
                analysis_parameters.match_method == MatchCircle ? *output << std::setw ( 5 ) << "+ MC" : *output << std::setw ( 5 ) << "+ MT";

                *output << std::setw ( 9 ) << "HELT[m]";

                //Print type of match
                analysis_parameters.match_method == MatchCircle ? *output << std::setw ( 5 ) << "+ MC" : *output << std::setw ( 5 ) << "+ MT";

                *output << std::setw ( 9 ) << "GNTF"
                        << std::setw ( 9 ) << "VDTF" << std::endl;

                //Values of the criterion for each projection
                for ( unsigned int i = 0;  i < ( analysis_parameters.analyzed_projections.size() == 0 ? items_printed : n ); i++ )
                {
                        if ( analysis_parameters.analyzed_projections.size() == 0 )
                        {
                                sl [i].printSampleRatios ( i + 1, analysis_parameters.analysis_type, n_test, output );
                        }

                        else if ( sl [i].getAnalyzedProjectionSample() )
                        {
                                sl [i].printSampleRatios ( i + 1, analysis_parameters.analysis_type, n_test, output );
                        }
                }

                //Table 2
                *output << std::endl << "Results containg positions of the criteria:" << std::endl << std::endl;

                //Create header 2: positions
                *output	<< std::setw ( 4 ) << "#"
                        << std::setw ( 8 ) << "Proj"
                        << std::setw ( 7 ) << "Categ"
                        << std::setw ( 6 ) << "latP"
                        << std::setw ( 7 ) << "lonP"
                        << std::setw ( 6 ) << "lat0"
                        << std::setw ( 7 ) << "lon0"
                        << std::setw ( 6 ) << "CND"
                        << std::setw ( 6 ) << "HOMT"
                        << std::setw ( 6 ) << "HELT"
                        << std::setw ( 6 ) << "GNTF"
                        << std::setw ( 6 ) << "VDTF" << std::endl;

                //Table 2: Positions of the criterion for each projection
                for ( unsigned int i = 0;  i < ( analysis_parameters.analyzed_projections.size() == 0  ? items_printed : n ); i++ )
                {
                        if ( analysis_parameters.analyzed_projections.size() == 0 )
                        {
                                sl [i].printSamplePositions ( i + 1, analysis_parameters.analysis_type, output );
                        }

                        else if ( sl [i].getAnalyzedProjectionSample() )
                        {
                                sl [i].printSamplePositions ( i + 1, analysis_parameters.analysis_type, output );
                        }
                }

                *output << std::endl << "  ( * Sample with additionaly corrected rotation, -c is enabled. )" << std::endl << std::endl;

                //Table 3
                *output << std::endl << "Analyzed and reference points:" << std::endl << std::endl;

                //Create header
                *output	<< std::setw ( 3 ) << "#"
                        << std::setw ( 15 ) << "X_test"
                        << std::setw ( 15 ) << "Y_test"
                        << std::setw ( 13 ) << "Fi_ref"
                        << std::setw ( 13 ) << "La_ref" << std::endl;

                //Table 3: List of analyzed and reference points
                for ( unsigned int i = 0; i < n_test; i++ )
                {
                        //Print all analyzed points
                        *output << std::setw ( 3 ) << i << std::fixed << std::setprecision ( 3 ) << std::setw ( 15 ) << nl_test [i]->getX() << std::setw ( 15 ) <<
                                nl_test [i] ->getY() << std::fixed << std::setprecision ( 5 ) << std::setw ( 13 ) << nl_reference [i]->getLat( ) <<
                                std::setw ( 13 ) << nl_reference [i]->getLon() << std::endl;
                }

                *output << std::endl << "Scale, rotation and matched points for each projection:" << std::endl << std::endl;

                //Table 4: Scale, rotation, all matched points and outliers
                for ( unsigned int i = 0;  i < ( analysis_parameters.analyzed_projections.size() == 0 ? items_printed : n ); i++ )
                {
                        if ( analysis_parameters.analyzed_projections.size() == 0 )
                        {
                                sl[i].printSampleMatchedPoints ( nl_test, nl_reference, i + 1, analysis_parameters.analysis_type, output );
                        }

                        else if ( sl [i].getAnalyzedProjectionSample() )
                        {
                                sl [i].printSampleMatchedPoints ( nl_test, nl_reference, i + 1, analysis_parameters.analysis_type, output );
                        }
                }

                *output << std::endl;
                *output << std::endl;
        }
}


#endif



