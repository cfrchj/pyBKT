#define NPY_NO_DEPRECATED_API NPY_1_11_API_VERSION

#include <iostream>
#include <stdint.h>
#include <alloca.h>
#include <Eigen/Core>
#include <boost/python.hpp>
#include <boost/python/numpy.hpp>
#include <boost/python/ptr.hpp>
#include <Python.h>
#include <numpy/ndarrayobject.h>
//#include <numpy/arrayobject.h>

using namespace Eigen;
using namespace std;
using namespace boost::python;

namespace np = numpy;
namespace p = boost::python;

//original comment:
//"TODO if we aren't outputting gamma, don't need to write it to memory (just
//need t and t+1), so we can save the stack array for each HMM at the cost of
//a branch"

/*struct double_to_python_float
{
    static PyObject* convert(double const& d)
      {
        return boost::python::incref(
          boost::python::object(d).ptr());
      }
};*/
// TODO openmp version

#if PY_VERSION_HEX >= 0x03000000
void *
#else
void
#endif
init_numpy(){
  //Py_Initialize;
  import_array();
}
//numpy scalar converters.
template <typename T, NPY_TYPES NumPyScalarType>
struct enable_numpy_scalar_converter
{
 enable_numpy_scalar_converter()
 {
  // Required NumPy call in order to use the NumPy C API within another
  // extension module.
  // import_array();
  init_numpy();  boost::python::converter::registry::push_back(
   &convertible,
   &construct,
   boost::python::type_id<T>());
 } static void* convertible(PyObject* object)
 {
  // The object is convertible if all of the following are true:
  // - is a valid object.
  // - is a numpy array scalar.
  // - its descriptor type matches the type for this converter.
  return (
   object &&                          // Valid
   PyArray_CheckScalar(object) &&                // Scalar
   PyArray_DescrFromScalar(object)->type_num == NumPyScalarType // Match
  )
   ? object // The Python object can be converted.
   : NULL;
 } static void construct(
  PyObject* object,
  boost::python::converter::rvalue_from_python_stage1_data* data)
 {
  // Obtain a handle to the memory block that the converter has allocated
  // for the C++ type.
  namespace python = boost::python;
  typedef python::converter::rvalue_from_python_storage<T> storage_type;
  void* storage = reinterpret_cast<storage_type*>(data)->storage.bytes;  // Extract the array scalar type directly into the storage.
  PyArray_ScalarAsCtype(object, storage);  // Set convertible to indicate success.
  data->convertible = storage;
 }
};

numpy::ndarray run(dict& data, dict& model, numpy::ndarray& forward_messages){
    //TODO: check if parameters are null.
    //TODO: check that dicts have the required members.
    //TODO: check that all parameters have the right sizes.
    //TODO: i'm not sending any error messages.

    numpy::ndarray alldata = extract<numpy::ndarray>(data["data"]); //multidimensional array, so i need to keep extracting arrays.
    int bigT = len(alldata[0]); //this should be the number of columns in the alldata object. i'm assuming is 2d array.
    int num_subparts = len(alldata);

    numpy::ndarray allresources = extract<numpy::ndarray>(data["resources"]);

    numpy::ndarray starts = extract<numpy::ndarray>(data["starts"]);
    int num_sequences = len(starts);

    numpy::ndarray lengths = extract<numpy::ndarray>(data["lengths"]);

    numpy::ndarray learns = extract<numpy::ndarray>(model["learns"]);
    int num_resources = len(learns);

    numpy::ndarray forgets = extract<numpy::ndarray>(model["forgets"]);

    numpy::ndarray guesses = extract<numpy::ndarray>(model["guesses"]);

    numpy::ndarray slips = extract<numpy::ndarray>(model["slips"]);

    double prior = extract<double>(model["prior"]);

    Array2d initial_distn;
    initial_distn << 1-prior, prior;

    MatrixXd As(2,2*num_resources);
    for (int n=0; n<num_resources; n++) {
        double learn = extract<double>(learns[n]);
        double forget = extract<double>(forgets[n]);
        As.col(2*n) << 1-learn, learn;
        As.col(2*n+1) << forget, 1-forget;
    }


    // forward messages
    //numpy::ndarray all_forward_messages = extract<numpy::ndarray>(forward_messages);
    double * forward_messages_temp = new double[2*bigT];
    for (int i=0; i<2; i++) {
        for (int j=0; j<bigT; j++){
            forward_messages_temp[i* bigT +j] = extract<double>(forward_messages[i][j]);
        }
    }


    //// outputs

    double* all_predictions = new double[2 * bigT];
    Map<Array2Xd,Aligned> predictions(all_predictions,2,bigT);

    /* COMPUTATION */

    for (int sequence_index=0; sequence_index < num_sequences; sequence_index++) {
        // NOTE: -1 because Matlab indexing starts at 1
        int64_t sequence_start = extract<int64_t>(starts[sequence_index]) - 1;
        int64_t T = extract<int64_t>(lengths[sequence_index]);

        //int16_t *resources = allresources + sequence_start;
        Map<MatrixXd, Aligned> forward_messages(forward_messages_temp + 2*sequence_start,2,T);
        Map<MatrixXd, Aligned> predictions(all_predictions + 2*sequence_start,2,T);

        predictions.col(0) = initial_distn;
        for (int t=0; t<T-1; t++) {
            int64_t resources_temp = extract<int64_t>(allresources[sequence_start+t]);
            predictions.col(t+1) = As.block(0,2*(resources_temp-1),2,2) * forward_messages.col(t);
        }
    }

    numpy::ndarray all_predictions_arr = numpy::from_data(all_predictions, numpy::dtype::get_builtin<double>(), boost::python::make_tuple(2, bigT),
                                                                           boost::python::make_tuple(bigT * 8, 8), boost::python::object()).copy();
    delete all_predictions;
    delete forward_messages_temp;
    return(all_predictions_arr);
}

BOOST_PYTHON_MODULE(predict_onestep_states){
    //import_array();
    Py_Initialize();
    np::initialize();
	enable_numpy_scalar_converter<boost::int64_t, NPY_INT64>();
    //to_python_converter<double, double_to_python_float>();
    def("run", run);

}

