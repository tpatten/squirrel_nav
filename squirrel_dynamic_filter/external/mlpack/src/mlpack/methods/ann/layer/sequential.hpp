/**
 * @file sequential.hpp
 * @author Marcus Edel
 *
 * Definition of the Sequential class, which acts as a feed-forward fully
 * connected network container.
 *
 * mlpack is free software; you may redistribute it and/or modify it under the
 * terms of the 3-clause BSD license.  You should have received a copy of the
 * 3-clause BSD license along with mlpack.  If not, see
 * http://www.opensource.org/licenses/BSD-3-Clause for more information.
 */
#ifndef MLPACK_METHODS_ANN_LAYER_SEQUENTIAL_HPP
#define MLPACK_METHODS_ANN_LAYER_SEQUENTIAL_HPP

#include <mlpack/prereqs.hpp>

#include <boost/ptr_container/ptr_vector.hpp>

#include "../visitor/delete_visitor.hpp"
#include "../visitor/delta_visitor.hpp"
#include "../visitor/output_height_visitor.hpp"
#include "../visitor/output_parameter_visitor.hpp"
#include "../visitor/output_width_visitor.hpp"

#include "layer_types.hpp"
#include "add_merge.hpp"

namespace mlpack {
namespace ann /** Artificial Neural Network. */ {

/**
 * Implementation of the Sequential class. The sequential class works as a
 * feed-forward fully connected network container which plugs various layers
 * together.
 *
 * @tparam InputDataType Type of the input data (arma::colvec, arma::mat,
 *         arma::sp_mat or arma::cube).
 * @tparam OutputDataType Type of the output data (arma::colvec, arma::mat,
 *         arma::sp_mat or arma::cube).
 */
template <
    typename InputDataType = arma::mat,
    typename OutputDataType = arma::mat
>
class Sequential
{
 public:

  /**
   * Create the Sequential object using the specified parameters.
   *
   * @param model Expose the all network modules.
   */
  Sequential(const bool model = true);

  //! Destroy the Sequential object.
  ~Sequential();

  /**
   * Ordinary feed forward pass of a neural network, evaluating the function
   * f(x) by propagating the activity forward through f.
   *
   * @param input Input data used for evaluating the specified function.
   * @param output Resulting output activation.
   */
  template<typename eT>
  void Forward(arma::Mat<eT>&& input, arma::Mat<eT>&& output);

  /**
   * Ordinary feed backward pass of a neural network, using 3rd-order tensors as
   * input, calculating the function f(x) by propagating x backwards through f.
   * Using the results from the feed forward pass.
   *
   * @param input The propagated input activation.
   * @param gy The backpropagated error.
   * @param g The calculated gradient.
   */
  template<typename eT>
  void Backward(const arma::Mat<eT>&& /* input */,
                arma::Mat<eT>&& gy,
                arma::Mat<eT>&& g);

  /*
   * Calculate the gradient using the output delta and the input activation.
   *
   * @param input The input parameter used for calculating the gradient.
   * @param error The calculated error.
   * @param gradient The calculated gradient.
   */
  template<typename eT>
  void Gradient(arma::Mat<eT>&& input,
                arma::Mat<eT>&& error,
                arma::Mat<eT>&& /* gradient */);

  /*
   * Add a new module to the model.
   *
   * @param args The layer parameter.
   */
  template <class LayerType, class... Args>
  void Add(Args... args) { network.push_back(new LayerType(args...)); }

  /*
   * Add a new module to the model.
   *
   * @param layer The Layer to be added to the model.
   */
  void Add(LayerTypes layer) { network.push_back(layer); }

  //! Return the model modules.
  std::vector<LayerTypes>& Model()
  {
    if (model)
    {
      return network;
    }

    return empty;
  }

  //! Return the initial point for the optimization.
  const arma::mat& Parameters() const { return parameters; }
  //! Modify the initial point for the optimization.
  arma::mat& Parameters() { return parameters; }

  arma::mat const& InputParameter() const { return inputParameter; }
  //! Modify the input parameter.
  arma::mat& InputParameter() { return inputParameter; }

  //! Get the output parameter.
  arma::mat const& OutputParameter() const { return outputParameter; }
  //! Modify the output parameter.
  arma::mat& OutputParameter() { return outputParameter; }

  //! Get the delta.e
  arma::mat const& Delta() const { return delta; }
  //! Modify the delta.
  arma::mat& Delta() { return delta; }

  //! Get the gradient.
  arma::mat const& Gradient() const { return gradient; }
  //! Modify the gradient.
  arma::mat& Gradient() { return gradient; }

  /**
   * Serialize the layer
   */
  template<typename Archive>
  void Serialize(Archive& /* ar */, const unsigned int /* version */);

 private:
  //! Parameter which indicates if the modules should be exposed.
  bool model;

  //! Indicator if we already initialized the model.
  bool reset;

  //! Locally-stored network modules.
  std::vector<LayerTypes> network;

  //! Locally-stored model parameters.
  arma::mat parameters;

  //! Locally-stored delta visitor.
  DeltaVisitor deltaVisitor;

  //! Locally-stored output parameter visitor.
  OutputParameterVisitor outputParameterVisitor;

  //! Locally-stored delete visitor.
  DeleteVisitor deleteVisitor;

  //! Locally-stored empty list of modules.
  std::vector<LayerTypes> empty;

  //! Locally-stored delta object.
  arma::mat delta;

  //! Locally-stored input parameter object.
  arma::mat inputParameter;

  //! Locally-stored output parameter object.
  arma::mat outputParameter;

  //! Locally-stored gradient object.
  arma::mat gradient;

  //! Locally-stored output width visitor.
  OutputWidthVisitor outputWidthVisitor;

  //! Locally-stored output height visitor.
  OutputHeightVisitor outputHeightVisitor;

  //! The input width.
  size_t width;

  //! The input height.
  size_t height;
}; // class Sequential

} // namespace ann
} // namespace mlpack

// Include implementation.
#include "sequential_impl.hpp"

#endif
