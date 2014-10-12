// Description: Find global minimum using the differential evolution algorithm

// Copyright (c) 2010 - 2014
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


#ifndef DifferentialEvolution_HPP
#define DifferentialEvolution_HPP

#include <ctime>
#include <stdlib.h>

#include "libalgo/source/structures/matrix/Matrix.h"

#include "libalgo/source/algorithms/matrixoperations/MatrixOperations.h"

#include "libalgo/source/exceptions/ErrorMathMatrixDifferentSize.h"
#include "libalgo/source/exceptions/ErrorBadData.h"


template <typename T, typename Function>
T DifferentialEvolution::diffEvolution(Function function, Matrix <T> &XMIN,  Matrix <T> &XMAX, const unsigned int population_size,
	const T epsilon, const unsigned int max_gener, Matrix <T> F, T CR, const TMutationStrategy & mutation_strategy, const TAdaptiveControl &adaptive_control, Matrix <T> &W, Matrix <T> &X, Matrix <T> &Y, Matrix <T> &RES, Matrix <T> &XAVER, T &aver_res, T &max_res, unsigned int &gener, std::ostream * output)
{
	
	//Compute global minimum of the function dim <2,m> using the current differential evolution algorithm
	unsigned int dim = XMIN.cols(), row_index_min = 0, column_index_min = 0;
	bool improve = false;

	//Bad matrix size: throw exception
	if (XMIN.cols() != XMAX.cols())
		throw ErrorMathMatrixDifferentSize <Matrix <T> >("ErrorMathMatrixDifferentSize: ", " invalid dimension of the matrices, can not perform differential evolution; (rows_count columns_count):  ",
		XMIN, XMAX);

	//Too small population
	if (population_size < dim + 1)
		throw ErrorBadData("ErrorBadData: too small population (pop < dim + 1).", "Can not find the global minimum in this interval...");

	//Bad limits: throw exception
	for (unsigned int i = 0; i < dim; i++)
		if (XMIN(0, i) > XMAX(0, i)) throw ErrorBadData("ErrorBadData: all limits a(i) > b(i), should be a(i) < b(i).", "Can not find the global minimum in this interval...");

	//Create population matrices: matrix of arguments and matrix of values
	Matrix <T> P_A(population_size, dim), P_V(population_size, 1);

	//Create intial populaton
	createInitialPopulation(function, XMIN, XMAX, W, Y, RES, population_size, dim, P_A, P_V);
	
	//Initialize min and old min
	T min_res = MatrixOperations::min(P_V), min_res_old_100 = min_res, min_res_old = min_res;

	//Assign population attributes and values (residuals)
	Matrix <T> Q_A = P_A, Q_V = P_V;

	//Set generation to zero
	gener = 0;

	Matrix <T> FM(population_size, 1), CRM(population_size, 1);

	//Run differential evolution
	while (gener < max_gener)
	{
		//Process each element of the population: apply differential evolution operators to one element, slightly modified version
		//Current version differential evolution apply each operator to all population

		//Set the mutation and crossover factors depending on the adaptive control
		if (adaptive_control == AdaptiveDecreasing)
			F(0, 0) = 0.5 * (max_gener - gener) / max_gener;

		T dg = 1.2;

		for (unsigned int i = 0; i < population_size; i++)
		{
			//Set mutation and cross-over factors depending on the adaptive control
			if (adaptive_control == AdaptiveRandom)
			{
				F(0, 0) = 0.5 * (1.0 + ((T)rand() / (RAND_MAX + 1)));
			}
			
			else if (adaptive_control == Jitter)
			{
				//Create vector of random numbers (dim, 1)
				for (unsigned int j = 0; j < dim; j++)
				{
					//Generate the random number (0,1)
					const T r = (T)rand() / (RAND_MAX + 1);

					F(0, j) = 0.5 * (1 + 0.001 * (r - 0.5));
				}
			}

			else if ( ( adaptive_control == MFDE ) && (gener > 0) )
			{
				//Generate the random number (0,1)
				const T r = (T)rand() / (RAND_MAX + 1);

				//Modify the mutation factor F: 
				//Improvement of the solution
				if (min_res < min_res_old) //Improvement
				{
					F(0, 0) = 1.5 * sqrt(r * r * dg);
				}

				//No improvement
				else
				{
					F(0, 0) = sqrt(r * r * dg) - 0.2;
				}
				
				//Decrement dg
				dg -= 1.0 / population_size;
			}

			else if (adaptive_control == SAM)
			{
				const T TAU1 = 0.1;
				const T r1 = (T)rand() / (RAND_MAX + 1);

				//Update mutation factor
				if (r1 < TAU1)
				{
					const T r2 = (T)rand() / (RAND_MAX + 1);
					F(0, 0) = 0.1 + 0.9  * r2;
					FM(i, 0) = F(0, 0);
				}

				//Use the mutation factor from the previous generation
				else F(0, 0) = FM(i, 0);

				const T r3 = (T)rand() / (RAND_MAX + 1);
				
				//Update cross-over ratio
				if (r3 < TAU1)
				{
					const T r4 = (T)rand() / (RAND_MAX + 1);
					CR = r4;
					CRM(i, 0) = CR;
				}

				else CR = CRM(i, 0);
			}

			//Set mutation strategy, create mutated vector U
			Matrix <T> U(1, dim), AS(3, dim);

			if (mutation_strategy == DERand1Strategy)
				mutationStrategyDERand1(P_A, i, population_size, F, U);
			else  if (mutation_strategy == DERand2Strategy)
				mutationStrategyDERand2(P_A, i, population_size, F, U);
			else  if (mutation_strategy == DERandDir1Strategy)
				mutationStrategyDERandDir1(P_A, P_V,  i, population_size, F, U);
			else  if (mutation_strategy == DERandDir2Strategy)
				mutationStrategyDERandDir2(P_A, P_V, i, population_size, F, U);
			else if (mutation_strategy == DERandBest1Strategy)
				mutationStrategyDERandBest1(P_A, P_V, i, population_size, F, U);
			else if (mutation_strategy == DERandBest2Strategy)
				mutationStrategyDERandBest2(P_A, P_V, i, population_size, F, U);
			else if (mutation_strategy == DERandBestDir1Strategy)
				mutationStrategyDERandBestDir1(P_A, P_V, i, population_size, F, U);
			else if (mutation_strategy == DETargetToBest1Strategy)
				mutationStrategyDETargetToBest1(P_A, P_V, i, population_size, F, U);
			else if (mutation_strategy == SACPStrategy)
				mutationStrategySACP(function, P_A, P_V, i, population_size, aver_res, F, CR, U);

			//Perform cross-over, create vector V
			Matrix <T> V = P_A.row(i); //Assign V = X
			crossover(U, CR, dim, V);

			//Perform redlection in to the search space
			reflection(XMIN, XMAX, dim, V);

			//Compute residuals for perturbated vector V
			T function_val_y;

			try
			{
				function(V, Y, RES, W);
				function_val_y = (MatrixOperations::trans(RES) * W * RES) (0, 0);
			}

			catch (Error & error)
			{
				function_val_y = MAX_FLOAT;
			}

			//Replacement rule: new value is better, update Q
			if (function_val_y <= P_V(i, 0))
			{
				Q_A.row(V, i);
				Q_V(i, 0) = function_val_y;
			}
		}

		//Replace P with Q: P elements can not be overwritten inside the cycle
		P_A = Q_A;
		P_V = Q_V;

		//Compute average of the population
		XAVER = sumCols(P_A) * 1.0 / population_size;

		//Actualize new maximum, minimum and average of the population
		min_res_old = min_res;
		max_res = MatrixOperations::max(P_V);
		min_res = MatrixOperations::min(P_V, row_index_min, column_index_min);
		aver_res = MatrixOperations::sumCol(P_V, 0) / population_size;

		//Compute residual difference for population
		T diff = max_res - min_res;

		//Increment generation
		gener++;

		//Terminal condition: population diversity, no improvement during the last 100 generations
		if ((diff < epsilon * std::max(1.0, min_res)) /*&& (min_res < 1.0e2)*/|| ((gener % 100 == 0) && (fabs(min_res - min_res_old_100) < epsilon * std::max(1.0, min_res)) /*&& (min_res < 1.0e2)*/))
		{
			std::cout << "gener=" << gener;
			std::cout << std::fixed;
			std::cout << " res_min = " << std::setprecision(7) << min_res << "   res_max = " << std::setprecision(7) << max_res << "   res_aver = " << std::setprecision(7) << aver_res;
			std::cout << std::scientific;
			std::cout << "   res_dif = " << diff << '\n';			
			//RE.print();
			break;
		}

		//Remeber minimal value for every 100-th generation
		if (gener % 100 == 0)
		{
			min_res_old_100 = min_res;
		}

		//Print "." for every 50-th generation
		if (gener % 50 == 0)
		{
			std::cout << "gener=" << gener;
			std::cout << std::fixed;
			std::cout << " res_min = " << std::setprecision(7) << min_res << "   res_max = " << std::setprecision(7) << max_res << "   res_aver = " << std::setprecision(7) << aver_res;
			std::cout << std::scientific;
			std::cout<< "   res_dif = " << diff << '\n';
			std::cout.flush();
			std::cout << ".";

			//if (max_res - min_res < 0.1 * max_res)
			//	P_A.print();
			//P_A.print();
			//RE.print();
		}
		
		//Additional stopping condition
		//if (gener > 250)
		//{
		//	gener = 1000;
		//	break;
		//}

		//Matrix <T> XT = P_A.row(row_index_min);
		//XT.print();
		//P_A.print(output);
		//P_A.print();
		if (gener % 10 == 0)
		{
			//std::cout << max_res << "   " << min_res << '\n';
			//P_A.print();
		}
	}

	//Actualize minimum argument
	X = P_A.row(row_index_min);

	return min_res;
}


template <typename T, typename Function>
void DifferentialEvolution::createInitialPopulation(Function function, const Matrix <T> &XMIN, const Matrix <T> &XMAX, Matrix <T> &W,  Matrix <T> &Y, Matrix <T> &RES, const unsigned int population_size, const unsigned int dim, Matrix <T> &P_A, Matrix <T> &P_V)
{
	//Create initial population

	//Initialize random number generator
	srand((unsigned)time(0));

	//Create individual and comoute its cost
	for (unsigned int i = 0; i < population_size; i++)
	{
		//Create random vectors
		for (unsigned int j = 0; j < dim; j++)
			P_A(i, j) = XMIN(0, j) + (XMAX(0, j) - XMIN(0, j)) * ((T)rand() / (RAND_MAX + 1));

		//Compute function values for random population
		try
		{
			function(P_A.row(i), Y, RES, W);
			P_V(i, 0) = (MatrixOperations::trans(RES) * W * RES) (0, 0);
		}

		catch (Error & error)
		{
			P_V(i, 0) = 0;
		}
	}
}


/*
template <typename T, typename Function>
T DifferentialEvolution::diffEvolutionDith ( Function function, const Matrix <T> &A, const Matrix <T> &B, const unsigned int population_size,
                                      const T epsilon, const unsigned int max_gener, T F, const T CR, const TMutationStrategy & sel_strategy, Matrix <T> &W, Matrix <T> &X, Matrix <T> &Y,  Matrix <T> &RES, T &aver_res, T &max_res, unsigned int &gener )
{
        //Compute global minimum of the function dim <2,m> using the differential evolution algorithm
	//The dithering method with the variable mutation factor F (Ching-Wei Chien et al. 2011)
        unsigned int dim = A.cols(), row_index_min = 0, column_index_min = 0;

        //Bad matrix size: throw exception
        if ( A.cols() != B.cols() )
                throw ErrorMathMatrixDifferentSize <Matrix <T> > ( "ErrorMathMatrixDifferentSize: ", " invalid dimension of the matrices, can not perform differential evolution; (rows_count columns_count):  ",
                                A, B );

        //Too small population
        if ( population_size < dim + 1 )
                throw ErrorBadData ( "ErrorBadData: too small population (pop < dim + 1).", "Can not find the global minimum in this interval..." );

        //Bad limits: throw exception
        for ( unsigned int i = 0;  i < dim ; i++ )
                if ( A ( 0, i ) > B ( 0, i ) ) throw ErrorBadData ( "ErrorBadData: all limits a(i) > b(i), should be a(i) < b(i).", "Can not find the global minimum in this interval..." );

        //Initialize random number generator
        srand ( ( unsigned ) time ( 0 ) );

        //Create population matrices: matrix of arguments and matrix of values
        Matrix <T> P_A ( population_size, dim ), P_V ( population_size, 1 );

        //Compute intial populaton
        for ( unsigned int i = 0; i < population_size; i++ )
        {
                //Create random vectors
                for ( unsigned int j = 0; j < dim; j++ )
                        P_A ( i, j ) = A ( 0, j ) + ( B ( 0, j ) - A ( 0, j ) ) * ( ( T ) rand() / ( RAND_MAX + 1 ) );

                //Compute function values for random population
                try
                {
                        function ( P_A.row ( i ), Y, RES, W );
                        P_V ( i, 0 ) = ( MatrixOperations::trans ( RES ) * W * RES ) ( 0, 0 );
                }

                catch ( Error & error )
                {
                        P_V ( i, 0 ) = MAX_FLOAT;
                }
        }

        //Initialize min and old min
        T min_res = MatrixOperations::min ( P_V ), min_res_old = min_val;

        //Assign population attributes and values (residuals)
        Matrix <T> Q_A = P_A, Q_V = P_V;

        //Set generation to zero
        gener = 0;

        //Initialize decrement of the mutation factor F (Ching-Wei Chien et al. 2011)
        const T dF = 1.0 / population_size;

        //Run differential evolution
        while ( gener < max_gener )
        {
                //Process each element of the population: apply differential evolution operators to one element, slightly modified version
                //Current version differential evolution apply each operator to all population
                for ( unsigned int i = 0; i < population_size; i++ )
                {
                        //Generate the random number (0,1)
                        const T r = ( T ) rand() / ( RAND_MAX + 1 );

                        //Modify the mutation factor F
                        F = 1.5 * sqrt ( r * r * ( 1.0 - dF ) ) - 0.2;

                        //Set mutation strategy, create mutated vector U
                        Matrix <T> U ( 1, dim );

			if (sel_strategy == DERand1Strategy)
				mutationStrategyDERand1(P_A, i, population_size, F, U);
			else  if (sel_strategy == DERand2Strategy)
				mutationStrategyDERand2(P_A, i, population_size, F, U);
			else  if (sel_strategy == DERandDir1Strategy)
				mutationStrategyDERandDir1(function, P_A, i, population_size, F, U);
			else  if (sel_strategy == DERandDir2Strategy)
				mutationStrategyDERandDir2(function, P_A, i, population_size, F, U);
			else if (sel_strategy == DERandBest1Strategy)
				mutationStrategyDERandBest1(P_A, P_V, i, population_size, F, U);
			else if (sel_strategy == DERandBest2Strategy)
				mutationStrategyDERandBest2(P_A, P_V, i, population_size, F, U);
			else if (sel_strategy == DERandBestDir1Strategy)
				mutationStrategyDERandBestDir1(P_A, P_V, i, population_size, F, U);

                        //Perform cross-over, create vector V
                        Matrix <T> V = P_A.row ( i );
                        crossover ( U, CR, dim, V );

                        //Perform redlection in to the search space
                        reflection ( A, B, dim, V );

                        //Compute residuals for perturbated vector V
                        T function_val_y;

                        try
                        {
                                function ( V, Y, RES, W );
                                function_val_y = ( MatrixOperations::trans ( RES ) * W * RES ) ( 0, 0 ) ;
                        }

                        catch ( Error & error )
                        {
                                function_val_y = MAX_FLOAT;
                        }

                        //Replacement rule: new value is better, update Q
                        if ( function_val_y < P_V ( i, 0 ) )
                        {
                                Q_A.row ( V, i );
                                Q_V ( i, 0 ) = function_val_y;
                        }
                }

                //Replace P with Q: P elements can not be overwritten inside the cycle
                P_A = Q_A;
                P_V = Q_V;

                //Actualize new maximum, minimum and average
		max_res = MatrixOperations::max(P_V);
		min_res = MatrixOperations::min(P_V, row_index_min, column_index_min);
		aver_res = MatrixOperations::sum(P_V, 0) / population_size;

		//Compute residual difference for population
		const T diff = max_res - min_res;

                //Increment generation
                gener++;

                //Terminal condition: population diversity, no improvement during the last 100 generations
                if ( (diff < epsilon ) || ( ( gener % 200 == 0 ) && ( fabs ( min_res - min_res_old ) < epsilon ) ) )
                {
			std::cout << "gener=" << gener;
			std::cout << std::fixed;
			std::cout << " res_min = " << std::setprecision(7) << min_res << "   res_max = " << std::setprecision(7) << max_res << "   res_aver = " << std::setprecision(7) << res_aver << '\n';
			//RE.print();
                        break;
                }

                //Remeber minimal value for every 100-th generation
                if ( gener % 200 == 0 )
                {
                        min_res_old = min_res;
                }

                //Print "." for every 50-th generation
                if ( gener % 50 == 0 )
                {
			std::cout << "gener=" << gener;
			std::cout << std::fixed;
			std::cout << " res_min = " << std::setprecision(7) << min_res << "   res_max = " << std::setprecision(7) << max_res << "   res_aver = " << std::setprecision(7) << res_aver << '\n';
			std::cout.flush();
                        std::cout << ".";
                        //P_A.print();
                        //RE.print();
                }

		Matrix <T> XT = P_A.row(row_index_min);
        }

        //Actualize minimum argument
        X = P_A.row ( row_index_min );

        return min_res;
}
*/


template <typename T>
void DifferentialEvolution::mutationStrategyDERand1(const Matrix <T> &P_A, const unsigned int & i, const unsigned int & population_size, const Matrix <T> & F, Matrix <T> &U)
{
        //Create next generation picking 3 random random vectors from the current population
        unsigned int i1 = 0, i2 = 0, i3 = 0, m = F.rows();

        //Get three random indices different from i

        do ( i1 = rand() % population_size ); while ( i1 == i );

        do ( i2 = rand() % population_size ); while ( ( i2 == i1 ) || ( i2 == i ) );

        do ( i3 = rand() % population_size ); while ( ( i3 == i2 ) || ( i3 == i1 ) || ( i3 == i ) );

        //Get vectors corresponding to indices
        Matrix <T> R1 = P_A.row ( i1 ), R2 = P_A.row ( i2 ), R3 = P_A.row ( i3 );

        //Create new vector u from R1, R2, R3
	if (m == 1)
		U = R1 + F(0, 0) *(R2 - R3);

	//Jitter version: dot product
	else
		U = R1 + F % (R2 - R3);
}


template <typename T>
void DifferentialEvolution::mutationStrategyDERand2(const Matrix <T> &P_A, const unsigned int & i, const unsigned int & population_size, const Matrix <T> & F, Matrix <T> &U)
{
	//Create next generation picking 5 random random vectors from the current population
	unsigned int i1 = 0, i2 = 0, i3 = 0, i4 = 0, i5 = 0, m = F.rows();

	//Get three random indices different from i

	do (i1 = rand() % population_size); while (i1 == i);

	do (i2 = rand() % population_size); while ((i2 == i1) || (i2 == i));

	do (i3 = rand() % population_size); while ((i3 == i2) || (i3 == i1) || (i3 == i));

	do (i4 = rand() % population_size); while ((i4 == i3) || (i4 == i2) || (i4 == i1) || (i4 == i));

	do (i5 = rand() % population_size); while ((i5 == i4) || (i5 == i3) || (i5 == i2) || (i5 == i1) || (i5 == i));

	//Get vectors corresponding to indices
	Matrix <T> R1 = P_A.row(i1), R2 = P_A.row(i2), R3 = P_A.row(i3), R4 = P_A.row(i4), R5 = P_A.row(i5);

	//Create new vector u from R1, R2, R3
	if (m == 1)
		U = R1 + F(0, 0) *(R2 + R4 - R3 - R5); 
	
	//Jitter version: dot product
	else
		U = R1 + F % (R2 + R4 - R3 - R5); 
}


template <typename T>
void DifferentialEvolution::mutationStrategyDERandDir1(const Matrix <T> &P_A, const Matrix <T> &P_V, const unsigned int & i, const unsigned int & population_size, const Matrix <T> & F, Matrix <T> &U)
{
	//Create next generation picking 2 random random vectors from the current population + aproximate gradient
	unsigned int i1 = 0, i2 = 0;
	const unsigned int m = P_A.rows(), n = P_A.cols(), nf = F.cols();

	//Get three random indices different from i

	do (i1 = rand() % population_size); while (i1 == i);

	do (i2 = rand() % population_size); while ((i2 == i1) || (i2 == i));

	//Get vectors corresponding to indices  and objective function values
	Matrix <T> R1 = P_A.row(i1), R2 = P_A.row(i2);
	T function_val_y1 = P_V(i1, 0), function_val_y2 = P_V(i2, 0);

	//Create new vector u from R1, R2, approximation of the gradient
	if (function_val_y1 <= function_val_y2)
	{
		if (nf == 1)
			U = R1 + F(0, 0) *(R1 - R2);

		//Jitter version: dot product
		else
			U = R1 + F % (R1 - R2);
	}

	else
	{
		if ( nf == 1)
			U = R2 + F(0, 0) *(R2 - R1);

		//Jitter version: dot product
		else
			U = R2 + F % (R2 - R1);
	}
}


template <typename T>
void DifferentialEvolution::mutationStrategyDERandDir2(const Matrix <T> &P_A, const Matrix <T> &P_V, const unsigned int & i, const unsigned int & population_size, const Matrix <T> & F, Matrix <T> &U)
{
	//Create next generation picking 4 random sorted vectors from the current population + approximate gradient
	unsigned int i1 = 0, i2 = 0, i3 = 0, i4 = 0;
	const unsigned int m = P_A.rows(), n = P_A.cols(), nf = F.cols();

	//Get three random indices different from i

	do (i1 = rand() % population_size); while (i1 == i);

	do (i2 = rand() % population_size); while ((i2 == i1) || (i2 == i));

	do (i3 = rand() % population_size); while ((i3 == i2) || (i3 == i1) || (i3 == i));

	do (i4 = rand() % population_size); while ((i4 == i3) || (i4 == i2) || (i4 == i1) || (i4 == i));

	//Get individuals corresponding to indices and their values
	Matrix <T> R1 = P_A.row(i1), R2 = P_A.row(i2), R3 = P_A.row(i3), R4 = P_A.row(i4);
	T function_val_y1 = P_V(i1, 0), function_val_y2 = P_V(i2, 0), function_val_y3 = P_V(i3, 0), function_val_y4 = P_V(i4, 0);

	//Compute mutation
	Matrix <T> v1 = R1, v2 = R2, v3 = R3, v4 = R4;
	if (function_val_y1 > function_val_y2)
	{
		v1 = R2;
		v2 = R1;
	}

	if (function_val_y3 > function_val_y4)
	{
		v3 = R4;
		v4 = R3;
	}

	//Create new vector u from v1, v2, v3, v4
	if ( nf == 1)
		U = v1 + F(0, 0) *(v1 - v2 + v3 - v4);

	//Jitter version: dot product
	else
		U = v1 + F % (v1 - v2 + v3 - v4);
}


template <typename T>
void  DifferentialEvolution::mutationStrategyDERandBest1(const Matrix <T> &P_A, const Matrix <T> &P_V, const unsigned int & i, const unsigned int & population_size, const Matrix <T> & F, Matrix <T> &U)
{
	//Create next generation picking the minimum and 4 random random vectors from the current population
	unsigned int i1 = 0, i2 = 0;

	//Find best argument
	unsigned int row_index_min = 0, column_index_min = 0, nf = F.cols();
	const T min_val = MatrixOperations::min(P_V, row_index_min, column_index_min);

	//Get four random indices different from i
	do (i1 = rand() % population_size); while ((i1 == i) || (i1 == row_index_min));

	do (i2 = rand() % population_size); while ((i2 == i1) || (i2 == i) || (i2 == row_index_min));

	//Get vectors corresponding to indices
	Matrix <T> R1 = P_A.row(i1), R2 = P_A.row(i2);

	//Create new vector u from R1, R2
	if ( nf == 1)
		U = P_A.row(row_index_min) + F(0, 0) * (R1 - R2);

	//Jitter version: dot product
	else
	{
		U = P_A.row(row_index_min) + F % (R1 - R2);
	}
}


template <typename T>
void  DifferentialEvolution::mutationStrategyDERandBest2(const Matrix <T> &P_A, const Matrix <T> &P_V, const unsigned int & i, const unsigned int & population_size, const Matrix <T> & F, Matrix <T> &U)
{
        //Create next generation picking the minimum and 4 random random vectors from the current population
        unsigned int i1 = 0, i2 = 0, i3 = 0, i4 = 0;

        //Find best argument
	unsigned int row_index_min = 0, column_index_min = 0, nf = F.cols();
        const T min_val = MatrixOperations::min ( P_V, row_index_min, column_index_min );

        //Get four random indices different from i
        do ( i1 = rand() % population_size ); while ( ( i1 == i ) || ( i1 == row_index_min ) );

        do ( i2 = rand() % population_size ); while ( ( i2 == i1 ) || ( i2 == i ) || ( i2 == row_index_min ) );

        do ( i3 = rand() % population_size ); while ( ( i3 == i2 ) || ( i3 == i1 ) || ( i3 == i ) || ( i3 == row_index_min ) );

        do ( i4 = rand() % population_size ); while ( ( i4 == i3 ) || ( i4 == i2 ) || ( i4 == i1 ) || ( i4 == i ) || ( i4 == row_index_min ) );

        //Get vectors corresponding to indices
        Matrix <T> R1 = P_A.row ( i1 ), R2 = P_A.row ( i2 ), R3 = P_A.row ( i3 ), R4 = P_A.row ( i4 );

        //Create new vector u from R1, R2, R3, R4, Rbest
	if ( nf == 1)
		U = P_A.row ( row_index_min ) + F(0, 0) * (R1 - R2 + R3 - R4);

	//Jitter version: dot product
	else
		U = P_A.row ( row_index_min ) + F % (R1 - R2 + R3 - R4);
}


template <typename T>
void  DifferentialEvolution::mutationStrategyDERandBestDir1(const Matrix <T> &P_A, const Matrix <T> &P_V, const unsigned int & i, const unsigned int & population_size, const Matrix <T> & F, Matrix <T> &U)
{
	//Create next generation picking the minimum and 2 random random, best and current vectors from the population
	unsigned int i1 = 0, i2 = 0;

	//Find best argument
	unsigned int row_index_min = 0, column_index_min = 0, nf = F.cols();
	const T min_val = MatrixOperations::min(P_V, row_index_min, column_index_min);

	//Get four random indices different from i
	do (i1 = rand() % population_size); while ((i1 == i) || (i1 == row_index_min));

	do (i2 = rand() % population_size); while ((i2 == i1) || (i2 == i) || (i2 == row_index_min));
	
	//Get vectors corresponding to indices
	Matrix <T> R1 = P_A.row(i1), R2 = P_A.row(i2), Ri = P_A.row(i);

	//Create new vector u from R1, R2, R3
	if ( nf == 1)
		U = P_A.row(row_index_min) + F(0, 0) * (P_A.row(row_index_min) + R1 - Ri - R2 );

	//Jitter version: dot product
	else
		U = P_A.row(row_index_min) + F % (P_A.row(row_index_min) + R1 - Ri - R2);
}


template <typename T>
void DifferentialEvolution::mutationStrategyDETargetToBest1(const Matrix <T> &P_A, const Matrix <T> &P_V, const unsigned int & i, const unsigned int & population_size, const Matrix <T>  & F, Matrix <T> &U)
{
	//Create next generation picking the minimum and 2 random random, best and current vectors from the population
	unsigned int i1 = 0, i2 = 0;

	//Find best argument
	unsigned int row_index_min = 0, column_index_min = 0, nf = F.cols();
	const T min_val = MatrixOperations::min(P_V, row_index_min, column_index_min);

	//Get two random indices different from i
	do (i1 = rand() % population_size); while ((i1 == i) || (i1 == row_index_min));

	do (i2 = rand() % population_size); while ((i2 == i1) || (i2 == i) || (i2 == row_index_min));

	//Get vectors corresponding to indices
	Matrix <T> R1 = P_A.row(i1), R2 = P_A.row(i2), Ri = P_A.row(i);

	//Create new vector u from R1, R2, R3
	if (nf == 1)
		U = Ri + F(0, 0) * ( P_A.row(row_index_min) - Ri) + F(0, 0) * (R1 - R2);

	//Jitter version: dot product
	else
		U = Ri + F % (P_A.row(row_index_min) - Ri) + F % (R1 - R2);
}


template <typename T, typename Function>
void  DifferentialEvolution::mutationStrategySACP(Function function, const Matrix <T> &P_A, const Matrix <T> &P_V, const unsigned int & i, const unsigned int & population_size, const T &aver_res, Matrix <T> & F, T &CR, Matrix <T> &U)
{
	//Adaptive strategy for SACP method, modifying the mutation and cross-over factors F, CR
	unsigned int i1 = 0, i2 = 0, i3 = 0;
	const unsigned int m = P_A.rows(), n = P_A.cols();

	//Get three random indices different from i

	do (i1 = rand() % population_size); while (i1 == i);

	do (i2 = rand() % population_size); while ((i2 == i1) || (i2 == i));

	do (i3 = rand() % population_size); while ((i3 == i2) || (i3 == i1) || (i3 == i));

	//Create matrix of attributes corresponding to indices and their values
	Matrix <T> A(3, n), V(3, 1);
	A.submat(P_A.row(i1), 0, 0); A.submat(P_A.row(i2), 1, 0); A.submat(P_A.row(i3), 2, 0);
	V(0, 0) = P_V(i1, 0); V(1, 0) = P_V(i2, 0); V(2, 0) = P_V(i3, 0);
	
	//Sort residuals in the ascendent order, get the permutation matrix IX
	Matrix <unsigned int> IX(3, 1);
	sortrows(V, IX, 0);

	//Sorted attributes: apply permutation matrix IX
	Matrix <T> AS(3, n);
	AS.submat(A.row(IX(0, 0)), 0, 0);
	AS.submat(A.row(IX(1, 0)), 1, 0);
	AS.submat(A.row(IX(2, 0)), 2, 0);

	//Compute new mutation factor
	F(0, 0) = 0.1 + 0.8 * (V(1, 0) - V(0, 0)) / (V(2, 0) - V(0, 0));

	//Compute new cross-over ratio
	T function_val_yi = P_V(i, 0);
	if (function_val_yi >= aver_res)
	{
		CR = 0.1 + 0.5 * ( function_val_yi - V(0, 0)) / (V(2, 0) - V(0, 0) );
	}

	else CR = 0.1;

	//Apply mutation factor
	U = AS.row(0) + F(0, 0) * (AS.row(1) - AS.row(2));
}


template <typename T>
void DifferentialEvolution::crossover ( const Matrix <T> &U, const T & CR, const unsigned int dim, Matrix <T> &V )
{
        //Compute cross-over: rewrite U elements
        unsigned short total_swap = 0;

        for ( unsigned int j = 0; j < dim; j++ )
        {
                //Generate random number and compare to C
                const T r_val = ( ( T ) rand() / ( RAND_MAX + 1 ) );

                //Rewrite elements
                if ( r_val < CR )
                {
                        V ( 0, j ) = U ( 0, j );
                        total_swap++;
                }
        }

        //No cross over has been performed: rewrite random element
        if ( total_swap == 0 )
        {
                const unsigned int index = rand() % dim;
                V ( 0, index ) = U ( 0, index );
        }
}


template <typename T>
void DifferentialEvolution::reflection(const Matrix <T> &XMIN, const Matrix <T> &XMAX, const unsigned int dim, Matrix <T> &V)
{
        //Reflect elements of vectors into the search space represented by the n-dimensional cuboid
        for ( unsigned int j = 0; j < dim; j++ )
        {
		while ((V(0, j) < XMIN(0, j)) || (V(0, j) > XMAX(0, j)))
                {
			if (XMIN(0, j) == XMAX(0, j))
				V(0, j) = XMIN(0, j);
			else if (V(0, j) > XMAX(0, j))
				V(0, j) = 2 * XMAX(0, j) - V(0, j);
			else if (V(0, j) < XMIN(0, j))
				V(0, j) = 2 * XMIN(0, j) - V(0, j);
                }
        }
}


#endif
