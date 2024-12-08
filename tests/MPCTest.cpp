/**
 * @file MPCTest.cpp
 * @author Giulio Romualdi
 * @copyright Released under the terms of the BSD 3-Clause License
 * @date 2018
 */

// Catch2
#include <catch2/catch_test_macros.hpp>

// OsqpEigen
#include <OsqpEigen/OsqpEigen.h>

// eigen
#include <Eigen/Dense>

#include <cmath>
#include <fstream>
#include <iostream>

// colors
#define ANSI_TXT_GRN "\033[0;32m"
#define ANSI_TXT_MGT "\033[0;35m" // Magenta
#define ANSI_TXT_DFT "\033[0;0m" // Console default
#define GTEST_BOX "[     cout ] "
#define COUT_GTEST ANSI_TXT_GRN << GTEST_BOX // You could add the Default
#define COUT_GTEST_MGT COUT_GTEST << ANSI_TXT_MGT

void setDynamicsMatrices(Eigen::Matrix<c_float, 12, 12>& a, Eigen::Matrix<c_float, 12, 4>& b)
{
    a << 1., 0., 0., 0., 0., 0., 0.1, 0., 0., 0., 0., 0., 0., 1., 0., 0., 0., 0., 0., 0.1, 0., 0.,
        0., 0., 0., 0., 1., 0., 0., 0., 0., 0., 0.1, 0., 0., 0., 0.0488, 0., 0., 1., 0., 0., 0.0016,
        0., 0., 0.0992, 0., 0., 0., -0.0488, 0., 0., 1., 0., 0., -0.0016, 0., 0., 0.0992, 0., 0.,
        0., 0., 0., 0., 1., 0., 0., 0., 0., 0., 0.0992, 0., 0., 0., 0., 0., 0., 1., 0., 0., 0., 0.,
        0., 0., 0., 0., 0., 0., 0., 0., 1., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 1., 0.,
        0., 0., 0.9734, 0., 0., 0., 0., 0., 0.0488, 0., 0., 0.9846, 0., 0., 0., -0.9734, 0., 0., 0.,
        0., 0., -0.0488, 0., 0., 0.9846, 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0.9846;

    b << 0., -0.0726, 0., 0.0726, -0.0726, 0., 0.0726, 0., -0.0152, 0.0152, -0.0152, 0.0152, -0.,
        -0.0006, -0., 0.0006, 0.0006, 0., -0.0006, 0.0000, 0.0106, 0.0106, 0.0106, 0.0106, 0,
        -1.4512, 0., 1.4512, -1.4512, 0., 1.4512, 0., -0.3049, 0.3049, -0.3049, 0.3049, -0.,
        -0.0236, 0., 0.0236, 0.0236, 0., -0.0236, 0., 0.2107, 0.2107, 0.2107, 0.2107;
}

void setInequalityConstraints(Eigen::Matrix<c_float, 12, 1>& xMax,
                              Eigen::Matrix<c_float, 12, 1>& xMin,
                              Eigen::Matrix<c_float, 4, 1>& uMax,
                              Eigen::Matrix<c_float, 4, 1>& uMin)
{
    c_float u0 = 10.5916;

    // input inequality constraints
    uMin << 9.6 - u0, 9.6 - u0, 9.6 - u0, 9.6 - u0;

    uMax << 13 - u0, 13 - u0, 13 - u0, 13 - u0;

    // state inequality constraints
    xMin << -M_PI / 6, -M_PI / 6, -OsqpEigen::INFTY, -OsqpEigen::INFTY, -OsqpEigen::INFTY, -1.,
        -OsqpEigen::INFTY, -OsqpEigen::INFTY, -OsqpEigen::INFTY, -OsqpEigen::INFTY,
        -OsqpEigen::INFTY, -OsqpEigen::INFTY;

    xMax << M_PI / 6, M_PI / 6, OsqpEigen::INFTY, OsqpEigen::INFTY, OsqpEigen::INFTY,
        OsqpEigen::INFTY, OsqpEigen::INFTY, OsqpEigen::INFTY, OsqpEigen::INFTY, OsqpEigen::INFTY,
        OsqpEigen::INFTY, OsqpEigen::INFTY;
}

void setWeightMatrices(Eigen::DiagonalMatrix<c_float, 12>& Q, Eigen::DiagonalMatrix<c_float, 4>& R)
{
    Q.diagonal() << 0, 0, 10., 10., 10., 10., 0, 0, 0, 5., 5., 5.;
    R.diagonal() << 0.1, 0.1, 0.1, 0.1;
}

void castMPCToQPHessian(const Eigen::DiagonalMatrix<c_float, 12>& Q,
                        const Eigen::DiagonalMatrix<c_float, 4>& R,
                        int mpcWindow,
                        Eigen::SparseMatrix<c_float>& hessianMatrix)
{

    hessianMatrix.resize(12 * (mpcWindow + 1) + 4 * mpcWindow,
                         12 * (mpcWindow + 1) + 4 * mpcWindow);

    // populate hessian matrix
    for (int i = 0; i < 12 * (mpcWindow + 1) + 4 * mpcWindow; i++)
    {
        if (i < 12 * (mpcWindow + 1))
        {
            int posQ = i % 12;
            float value = Q.diagonal()[posQ];
            if (value != 0)
                hessianMatrix.insert(i, i) = value;
        } else
        {
            int posR = i % 4;
            float value = R.diagonal()[posR];
            if (value != 0)
                hessianMatrix.insert(i, i) = value;
        }
    }
}

void castMPCToQPGradient(const Eigen::DiagonalMatrix<c_float, 12>& Q,
                         const Eigen::Matrix<c_float, 12, 1>& xRef,
                         int mpcWindow,
                         Eigen::Matrix<c_float, -1, 1>& gradient)
{

    Eigen::Matrix<c_float, 12, 1> Qx_ref;
    Qx_ref = Q * (-xRef);

    // populate the gradient vector
    gradient = Eigen::Matrix<c_float, -1, 1>::Zero(12 * (mpcWindow + 1) + 4 * mpcWindow, 1);
    for (int i = 0; i < 12 * (mpcWindow + 1); i++)
    {
        int posQ = i % 12;
        float value = Qx_ref(posQ, 0);
        gradient(i, 0) = value;
    }
}

void castMPCToQPConstraintMatrix(const Eigen::Matrix<c_float, 12, 12>& dynamicMatrix,
                                 const Eigen::Matrix<c_float, 12, 4>& controlMatrix,
                                 int mpcWindow,
                                 Eigen::SparseMatrix<c_float>& constraintMatrix)
{
    constraintMatrix.resize(12 * (mpcWindow + 1) + 12 * (mpcWindow + 1) + 4 * mpcWindow,
                            12 * (mpcWindow + 1) + 4 * mpcWindow);

    // populate linear constraint matrix
    for (int i = 0; i < 12 * (mpcWindow + 1); i++)
    {
        constraintMatrix.insert(i, i) = -1;
    }

    for (int i = 0; i < mpcWindow; i++)
        for (int j = 0; j < 12; j++)
            for (int k = 0; k < 12; k++)
            {
                float value = dynamicMatrix(j, k);
                if (value != 0)
                {
                    constraintMatrix.insert(12 * (i + 1) + j, 12 * i + k) = value;
                }
            }

    for (int i = 0; i < mpcWindow; i++)
        for (int j = 0; j < 12; j++)
            for (int k = 0; k < 4; k++)
            {
                float value = controlMatrix(j, k);
                if (value != 0)
                {
                    constraintMatrix.insert(12 * (i + 1) + j, 4 * i + k + 12 * (mpcWindow + 1))
                        = value;
                }
            }

    for (int i = 0; i < 12 * (mpcWindow + 1) + 4 * mpcWindow; i++)
    {
        constraintMatrix.insert(i + (mpcWindow + 1) * 12, i) = 1;
    }
}

void castMPCToQPConstraintVectors(const Eigen::Matrix<c_float, 12, 1>& xMax,
                                  const Eigen::Matrix<c_float, 12, 1>& xMin,
                                  const Eigen::Matrix<c_float, 4, 1>& uMax,
                                  const Eigen::Matrix<c_float, 4, 1>& uMin,
                                  const Eigen::Matrix<c_float, 12, 1>& x0,
                                  int mpcWindow,
                                  Eigen::Matrix<c_float, -1, 1>& lowerBound,
                                  Eigen::Matrix<c_float, -1, 1>& upperBound)
{
    // evaluate the lower and the upper inequality vectors
    Eigen::Matrix<c_float, -1, 1> lowerInequality
        = Eigen::Matrix<c_float, -1, -1>::Zero(12 * (mpcWindow + 1) + 4 * mpcWindow, 1);
    Eigen::Matrix<c_float, -1, 1> upperInequality
        = Eigen::Matrix<c_float, -1, -1>::Zero(12 * (mpcWindow + 1) + 4 * mpcWindow, 1);
    for (int i = 0; i < mpcWindow + 1; i++)
    {
        lowerInequality.block(12 * i, 0, 12, 1) = xMin;
        upperInequality.block(12 * i, 0, 12, 1) = xMax;
    }
    for (int i = 0; i < mpcWindow; i++)
    {
        lowerInequality.block(4 * i + 12 * (mpcWindow + 1), 0, 4, 1) = uMin;
        upperInequality.block(4 * i + 12 * (mpcWindow + 1), 0, 4, 1) = uMax;
    }

    // evaluate the lower and the upper equality vectors
    Eigen::Matrix<c_float, -1, 1> lowerEquality
        = Eigen::Matrix<c_float, -1, -1>::Zero(12 * (mpcWindow + 1), 1);
    Eigen::Matrix<c_float, -1, 1> upperEquality;
    lowerEquality.block(0, 0, 12, 1) = -x0;
    upperEquality = lowerEquality;
    lowerEquality = lowerEquality;

    // merge inequality and equality vectors
    lowerBound = Eigen::Matrix<c_float, -1, -1>::Zero(2 * 12 * (mpcWindow + 1) + 4 * mpcWindow, 1);
    lowerBound << lowerEquality, lowerInequality;

    upperBound = Eigen::Matrix<c_float, -1, -1>::Zero(2 * 12 * (mpcWindow + 1) + 4 * mpcWindow, 1);
    upperBound << upperEquality, upperInequality;
}

void updateConstraintVectors(const Eigen::Matrix<c_float, 12, 1>& x0,
                             Eigen::Matrix<c_float, -1, 1>& lowerBound,
                             Eigen::Matrix<c_float, -1, 1>& upperBound)
{
    lowerBound.block(0, 0, 12, 1) = -x0;
    upperBound.block(0, 0, 12, 1) = -x0;
}

c_float
getErrorNorm(const Eigen::Matrix<c_float, 12, 1>& x, const Eigen::Matrix<c_float, 12, 1>& xRef)
{
    // evaluate the error
    Eigen::Matrix<c_float, 12, 1> error = x - xRef;

    // return the norm
    return error.norm();
}

TEST_CASE("MPCTest")
{
    // open the ofstream
    std::ofstream dataStream;
    dataStream.open("output.txt");

    // set the preview window
    int mpcWindow = 20;

    // allocate the dynamics matrices
    Eigen::Matrix<c_float, 12, 12> a;
    Eigen::Matrix<c_float, 12, 4> b;

    // allocate the constraints vector
    Eigen::Matrix<c_float, 12, 1> xMax;
    Eigen::Matrix<c_float, 12, 1> xMin;
    Eigen::Matrix<c_float, 4, 1> uMax;
    Eigen::Matrix<c_float, 4, 1> uMin;

    // allocate the weight matrices
    Eigen::DiagonalMatrix<c_float, 12> Q;
    Eigen::DiagonalMatrix<c_float, 4> R;

    // allocate the initial and the reference state space
    Eigen::Matrix<c_float, 12, 1> x0;
    Eigen::Matrix<c_float, 12, 1> xRef;

    // allocate QP problem matrices and vectors
    Eigen::SparseMatrix<c_float> hessian;
    Eigen::Matrix<c_float, -1, 1> gradient;
    Eigen::SparseMatrix<c_float> linearMatrix;
    Eigen::Matrix<c_float, -1, 1> lowerBound;
    Eigen::Matrix<c_float, -1, 1> upperBound;

    // set the initial and the desired states
    x0 << 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;
    xRef << 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0;

    // set MPC problem quantities
    setDynamicsMatrices(a, b);
    setInequalityConstraints(xMax, xMin, uMax, uMin);
    setWeightMatrices(Q, R);

    // cast the MPC problem as QP problem
    castMPCToQPHessian(Q, R, mpcWindow, hessian);
    castMPCToQPGradient(Q, xRef, mpcWindow, gradient);
    castMPCToQPConstraintMatrix(a, b, mpcWindow, linearMatrix);
    castMPCToQPConstraintVectors(xMax, xMin, uMax, uMin, x0, mpcWindow, lowerBound, upperBound);

    // instantiate the solver
    OsqpEigen::Solver solver;

    // settings
    solver.settings()->setVerbosity(false);
    solver.settings()->setWarmStart(true);

    // set the initial data of the QP solver
    solver.data()->setNumberOfVariables(12 * (mpcWindow + 1) + 4 * mpcWindow);
    solver.data()->setNumberOfConstraints(2 * 12 * (mpcWindow + 1) + 4 * mpcWindow);
    REQUIRE(solver.data()->setHessianMatrix(hessian));
    REQUIRE(solver.data()->setGradient(gradient));
    REQUIRE(solver.data()->setLinearConstraintsMatrix(linearMatrix));
    REQUIRE(solver.data()->setLowerBound(lowerBound));
    REQUIRE(solver.data()->setUpperBound(upperBound));

    // instantiate the solver
    REQUIRE(solver.initSolver());

    // controller input and QPSolution vector
    Eigen::Matrix<c_float, 4, 1> ctr;
    Eigen::Matrix<c_float, -1, 1> QPSolution;

    // number of iteration steps
    int numberOfSteps = 50;

    // profiling quantities
    clock_t startTime, endTime;
    c_float averageTime = 0;

    for (int i = 0; i < numberOfSteps; i++)
    {
        startTime = clock();

        // solve the QP problem
        REQUIRE(solver.solveProblem() == OsqpEigen::ErrorExitFlag::NoError);

        // get the controller input
        QPSolution = solver.getSolution();
        ctr = QPSolution.block(12 * (mpcWindow + 1), 0, 4, 1);

        // save data into file
        auto x0Data = x0.data();
        for (int j = 0; j < 12; j++)
            dataStream << x0Data[j] << " ";
        dataStream << std::endl;

        // propagate the model
        x0 = a * x0 + b * ctr;

        // update the constraint bound
        updateConstraintVectors(x0, lowerBound, upperBound);
        REQUIRE(solver.updateBounds(lowerBound, upperBound));

        endTime = clock();

        averageTime += static_cast<double>(endTime - startTime) / CLOCKS_PER_SEC;
    }

    // close the stream
    dataStream.close();

    std::cout << COUT_GTEST_MGT << "Average time = " << averageTime / numberOfSteps << " seconds."
              << ANSI_TXT_DFT << std::endl;

    constexpr double tolerance = 1e-2;
    REQUIRE(getErrorNorm(x0, xRef) <= tolerance);
}
