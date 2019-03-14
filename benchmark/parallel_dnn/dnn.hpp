#pragma once

#include <iostream>
#include <experimental/filesystem>
#include <fstream>
#include <random>

#define EIGEN_DONT_PARALLELIZE 1

#include <Eigen/Dense>

// Function: read_mnist_label
inline auto read_mnist_label(const std::experimental::filesystem::path& path) {
  
  // Helper lambda.
  auto reverse_int = [](int i) {
    unsigned char c1, c2, c3, c4;
    c1 = i         & 255;
    c2 = (i >> 8)  & 255;
    c3 = (i >> 16) & 255;
    c4 = (i >> 24) & 255;
    return ((int)c1 << 24) + ((int)c2 << 16) + ((int)c3 << 8) + c4;
  };
  
  // Read the image.
  std::ifstream ifs(path, std::ios::binary);
  
  if(!ifs) {
    assert(false);
  }

  int magic_number = 0;
  int num_imgs = 0;

  ifs.read((char*)&magic_number, sizeof(magic_number));
  magic_number = reverse_int(magic_number);

  ifs.read((char*)&num_imgs, sizeof(num_imgs));
  num_imgs = reverse_int(num_imgs);
  
  Eigen::VectorXi labels(num_imgs);
  for (int i = 0; i<num_imgs; ++i) {
    unsigned char temp = 0;  // must use unsigned
    ifs.read((char*)&temp, sizeof(temp));
    labels[i] = static_cast<int>(temp);
  }
  return labels;
}


inline auto read_mnist_image(const std::experimental::filesystem::path& path) {
  
  // Helper lambda.
  auto reverse_int = [] (int i) {
    unsigned char c1, c2, c3, c4;
    c1 = i         & 255;
    c2 = (i >> 8)  & 255;
    c3 = (i >> 16) & 255;
    c4 = (i >> 24) & 255;
    return ((int)c1 << 24) + ((int)c2 << 16) + ((int)c3 << 8) + c4;
  };
  
  // Read the image.
  std::ifstream ifs(path, std::ios::binary);

  if(!ifs) {
    assert(false);
  }

  int magic_number = 0;
  int num_imgs = 0;
  int num_rows = 0;
  int num_cols = 0;

  ifs.read((char*)&magic_number, sizeof(magic_number));
  magic_number = reverse_int(magic_number);

  ifs.read((char*)&num_imgs, sizeof(num_imgs));
  num_imgs = reverse_int(num_imgs);

  ifs.read((char*)&num_rows, sizeof(num_rows));
  num_rows = reverse_int(num_rows);

  ifs.read((char*)&num_cols, sizeof(num_cols));
  num_cols = reverse_int(num_cols);
 
  Eigen::MatrixXf images(num_imgs, num_rows*num_cols);

  for(int i = 0; i < num_imgs; ++i) {
    for(int r = 0; r < num_rows; ++r) {
      for(int c = 0; c < num_cols; ++c) {
        unsigned char p = 0;  // must use unsigned
        ifs.read((char*)&p, sizeof(p));
        images(i, r*num_cols + c) = static_cast<float>(p);
      }
    }
  }

  for(int i=0; i<images.rows(); i++) {
    for(int j=0; j<images.cols(); j++) {
      images(i, j) /= 255.0;
    }
  }
  return images;
}

inline auto time_diff(
  std::chrono::time_point<std::chrono::high_resolution_clock> &t1, 
  std::chrono::time_point<std::chrono::high_resolution_clock> &t2
) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
}



// ------------------------------------------------------------------------------------------------


enum class Activation {
  NONE,
  RELU,
  SIGMOID
};

// Procedure: sigmoid
inline void sigmoid(Eigen::MatrixXf& x) {
  x = ((1.0f + (-x).array().exp()).inverse()).matrix();
}

// Procedure: relu
inline void relu(Eigen::MatrixXf& x) {
  for(int j=0; j<x.cols(); ++j) {
    for(int i=0; i<x.rows(); ++i) {
      if(x(i, j) <= 0.0f) {
        x(i, j) = 0.0f;
      }   
    }   
  }
}

inline void activate(Eigen::MatrixXf& mat, Activation act) {
  switch(act) {
    case Activation::NONE:  return;
    case Activation::SIGMOID: sigmoid(mat); return;
    case Activation::RELU: relu(mat); return;
  };
}


// Function: drelu
inline void drelu(Eigen::MatrixXf& x) {
  for(int j=0; j<x.cols(); ++j) {
    for(int i=0; i<x.rows(); ++i) {
      x(i, j) = x(i, j) > 0.0f ? 1.0f : 0.0f;
    }
  }
}

// Function: dsigmoid
inline void dsigmoid(Eigen::MatrixXf& x) {
  x = x.array() * (1 - x.array());
}

inline void deactivate(Eigen::MatrixXf& mat, Activation act) {
  switch(act) {
    case Activation::NONE:    mat = Eigen::MatrixXf::Ones(mat.rows(), mat.cols()); return; 
    case Activation::SIGMOID: dsigmoid(mat); return ;
    case Activation::RELU:    drelu(mat); return;
  };
}


// ----------------------------------------------------------------------------

struct MNIST_DNN {
  // Ctor
  MNIST_DNN() = default;

  void add_layer(size_t in_degree, size_t out_degree, Activation act) {
    acts.emplace_back(act);
    Ys.emplace_back().resize(batch_size, out_degree);
    Ws.push_back(Eigen::MatrixXf::Random(in_degree, out_degree));
    Bs.push_back(Eigen::MatrixXf::Random(1, out_degree));

    dW.emplace_back().resize(in_degree, out_degree);
    dB.emplace_back().resize(1, out_degree);
  }

  void forward(size_t layer, const Eigen::MatrixXf& mat) {
    Ys[layer] = mat * Ws[layer] + Bs[layer].replicate(mat.rows(), 1);
    activate(Ys[layer], acts[layer]);
  }

  void loss(const Eigen::VectorXi& labels) {
    delta = Ys.back();
    delta = (delta - delta.rowwise().maxCoeff().replicate(1, delta.cols())).array().exp().matrix();
    delta = delta.cwiseQuotient(delta.rowwise().sum().replicate(1, delta.cols()));
    for(size_t i=beg_row, j=0; j<batch_size; i++, j++) {
      delta(j, labels[i]) -= 1.0;
    }
  }

  void backward(size_t layer, const Eigen::MatrixXf& Xin) {
    deactivate(Ys[layer], acts[layer]);
    delta = delta.cwiseProduct(Ys[layer]);
    dB[layer] = delta.colwise().sum();
    dW[layer] = Xin.transpose() * delta;

    if(layer > 0) {
      delta = delta * Ws[layer].transpose();
    }
  }

  void update(size_t layer) {
    Ws[layer] -= lrate*(dW[layer] + decay*Ws[layer]);
    Bs[layer] -= lrate*(dB[layer] + decay*Bs[layer]); 
  }

  // Testing images # = 10000 x 784 (28 x 28)
  void validate(Eigen::MatrixXf& test_images, Eigen::VectorXi& test_labels) {
    Eigen::MatrixXf res = test_images; 
    //auto t1 = std::chrono::high_resolution_clock::now();
    for(size_t i=0; i<acts.size(); i++) {
      res = res * Ws[i] + Bs[i].replicate(res.rows(), 1);
      if(acts[i] == Activation::RELU) {
        relu(res);
      }
      else if(acts[i] == Activation::SIGMOID) {
        sigmoid(res);
      }
    }
    //auto t2 = std::chrono::high_resolution_clock::now();
    //std::cout << "Infer runtime: " << time_diff(t1, t2) << " ms\n";

    size_t correct_num {0};
    for(int k=0; k<res.rows(); k++) {
      int pred ; 
      res.row(k).maxCoeff(&pred);
      if(pred == test_labels[k]) {
        correct_num ++;
      }
    }
    //std::cout << "Accuracy: " << correct_num << '/' << res.rows() << '\n';
  }

  // Parameter functions ------------------------------------------------------
  auto& epoch_num(unsigned e) {
    epoch = e;
    return *this;
  }
  auto& batch(size_t b) {
    batch_size = b;
    //assert(images->rows()%batch_size == 0);
    return *this;
  }
  auto& learning_rate(float l) {
    lrate = l;
    return *this;
  }

  auto num_layers() const {
    return acts.size();
  }

  std::vector<Eigen::MatrixXf> Ys;
  std::vector<Eigen::MatrixXf> Ws;
  std::vector<Eigen::MatrixXf> Bs;
  std::vector<Eigen::MatrixXf> dW;
  std::vector<Eigen::MatrixXf> dB;

  std::vector<Activation> acts;

  // Training images # = 60000 x 784 (28 x 28)
  //Eigen::MatrixXf* images;
  //Eigen::VectorXi* labels;
  Eigen::MatrixXf delta;

  int beg_row {0};

  float lrate {0.01f};
  float decay {0.01f};

  unsigned epoch {0};
  size_t batch_size {1};
};






inline Eigen::MatrixXf IMAGES;
inline Eigen::VectorXi LABELS;
inline Eigen::MatrixXf TEST_IMAGES;
inline Eigen::VectorXi TEST_LABELS;

// A workable DNN setting:
//   BATCH = 100 (or 50~100)
//   NUM_ITERATIONS = 60000/BATCH
//   dnn.add_layer(784, 100, Activation::RELU);
//   dnn.add_layer(100, 30, Activation::RELU);
//   dnn.add_layer(30, 10, Activation::NONE); 

// The total number of training data = 60000
inline constexpr size_t BATCH {6000};
//inline constexpr size_t BATCH {60};
inline constexpr size_t NUM_ITERATIONS {60000/BATCH};
inline constexpr size_t NUM_DNNS {10};

inline void init_dnn(MNIST_DNN& dnn) {
  //dnn.batch(BATCH).learning_rate(0.001);
  //dnn.add_layer(784, 100, Activation::RELU);
  //dnn.add_layer(100, 30, Activation::RELU);
  //dnn.add_layer(30, 10, Activation::NONE); 

  dnn.batch(BATCH).learning_rate(0.001);
  dnn.add_layer(784, 64, Activation::RELU);
  dnn.add_layer(64, 32, Activation::RELU);
  dnn.add_layer(32, 16, Activation::RELU);
  dnn.add_layer(16, 8, Activation::RELU);
  dnn.add_layer(8, 10, Activation::NONE); 
}

inline void forward_task(MNIST_DNN& D,  
  Eigen::MatrixXf& mat, 
  Eigen::VectorXi& vec) {

  for(size_t i=0; i<D.acts.size(); i++) {
    if(i == 0){
      D.forward(i, mat.middleRows(D.beg_row, D.batch_size));
    }
    else {
      D.forward(i, D.Ys[i-1]);
    }
  }

  D.loss(vec);
}

inline void backward_task(MNIST_DNN& D, size_t i, Eigen::MatrixXf& mat) {
  if(i > 0) {
    //D.backward(i, D.Ys[i-1].transpose());       
    D.backward(i, D.Ys[i-1]);       
  }
  else {
    //D.backward(i, mat.middleRows(D.beg_row, D.batch_size).transpose());
    D.backward(i, mat.middleRows(D.beg_row, D.batch_size));

    D.beg_row += D.batch_size;
    if(D.beg_row >= IMAGES.rows()) {
      D.beg_row = 0;
    }

  }
}

inline void shuffle(Eigen::MatrixXf& mat, Eigen::VectorXi& vec) {

  static thread_local std::mt19937 gen(0); 

  Eigen::PermutationMatrix<Eigen::Dynamic, Eigen::Dynamic> p(mat.rows());
  p.setIdentity();
  std::shuffle(p.indices().data(), p.indices().data() + p.indices().size(), gen);

  mat = p * mat;
  vec = p * vec;
}


inline void report_runtime(std::chrono::time_point<std::chrono::high_resolution_clock>& t1) {
  auto t2 = std::chrono::high_resolution_clock::now();
  std::cout << "One iteration runtime: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << " ms\n";
  t1 = std::chrono::high_resolution_clock::now();
}


void run_tbb(unsigned, unsigned);
void run_taskflow(unsigned, unsigned);
void run_omp(unsigned, unsigned);
void run_sequential(unsigned, unsigned);

