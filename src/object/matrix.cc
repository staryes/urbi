/*
 * Copyright (C) 2011, Gostai S.A.S.
 *
 * This software is provided "as is" without warranty of any kind,
 * either expressed or implied, including but not limited to the
 * implied warranties of fitness for a particular purpose.
 *
 * See the LICENSE file for more information.
 */

#include <urbi/object/matrix.hh>
#include <boost/numeric/ublas/lu.hpp>

#include <kernel/uvalue-cast.hh>

namespace urbi
{
  namespace object
  {

    bool
    operator==(const Matrix::value_type& e1, const Matrix::value_type& e2)
    {
      return
        (e1.size1() == e2.size1()
         && e1.size2() == e2.size2()
         && norm_inf (e1 - e2) == 0);
    }

    Matrix::Matrix()
      : value_()
    {
      proto_add(proto);
    }

    Matrix::Matrix(size_t i, size_t j)
      : value_(i, j)
    {
      proto_add(proto);
    }

    Matrix::Matrix(value_type v)
      : value_(v)
    {
      proto_add(proto);
    }

    Matrix::Matrix(const Matrix& model)
      : CxxObject()
      , super_comparable_type()
      , value_(model.value_)
    {
      proto_add(proto);
    }

    Matrix::Matrix(const rMatrix& model)
      : CxxObject()
      , value_(model->value_)
    {
      proto_add(proto);
    }

    Matrix::Matrix(const rList& model)
      : CxxObject()
      , value_()
    {
      proto_add(proto);
      fromList(model);
    }

    rMatrix
    Matrix::init(const objects_type& args)
    {
      rMatrix self = args[0]->as<Matrix>();
      if (!self)
        runner::raise_type_error(args[1], Matrix::proto);

      if (args.size() == 1)
        runner::raise_arity_error(0, 1);
      else if (args.size() == 2)
      {
        if (rList l = args[1]->as<List>())
        {
          self->fromList(l);
          return self;
        }
        else if (rMatrix v = args[1]->as<Matrix>())
        {
          self->value_ = v->value_;
          return self;
        }
      }
      else if (args.size() == 3)
      {
        self->value_ =
          boost::numeric::ublas::zero_matrix<ufloat>
          (from_urbi<unsigned>(args[1]), from_urbi<unsigned>(args[2]));
        return self;
      }
      objects_type effective_args(args.begin() + 1, args.end());
      return self->fromArgsList(effective_args);
    }

    rMatrix
    Matrix::create_zeros(rObject, int size1, int size2)
    {
      boost::numeric::ublas::zero_matrix<ufloat> res(size1, size2);
      return new Matrix(res);
    }

    rMatrix
    Matrix::create_identity(rObject, int size)
    {
      boost::numeric::ublas::identity_matrix<ufloat> res(size);
      return new Matrix(res);
    }

    rMatrix
    Matrix::create_scalars(rObject, int size1, int size2, ufloat v)
    {
      boost::numeric::ublas::scalar_matrix<ufloat> res(size1, size2, v);
      return new Matrix(res);
    }

    rMatrix
    Matrix::create_ones(rObject self, int size1, int size2)
    {
      return create_scalars(self, size1, size2, 1.0);
    }

    rMatrix
    Matrix::transpose() const
    {
      return new Matrix(trans(value_));
    }

    rMatrix
    Matrix::invert() const
    {
      using namespace boost::numeric::ublas;
      if (size1() != size2())
        FRAISE("expected square matrix, got %dx%d",
               size1(), size2());
      // Create a working copy of the input.
      value_type A(value_);
      value_type inverse(size1(), size2());
      // Create a permutation matrix for the LU-factorization.
      permutation_matrix<size_type> pm(A.size1());
      // Perform LU-factorization.
      if (lu_factorize(A, pm) != 0)
        FRAISE("non-invertible matrix: %s", *this);
      // Create identity matrix of "inverse".
      inverse.assign(identity_matrix<ufloat>(A.size1()));
      // Backsubstitute to get the inverse.
      lu_substitute(A, pm, inverse);
      return new Matrix(inverse);
    }

    rMatrix
    Matrix::dot_times(const rMatrix& m) const
    {
      value_type res(size1(), size2());
      for (size_t i = 0; i < size1(); ++i)
        for (size_t j = 0; j < size2(); ++j)
          res(i, j) = value_(i, j) * (*m)(i, j);
      return new Matrix(res);
    }

    rMatrix
    Matrix::fromArgsList(const objects_type& args)
    {
      rList first_row = args[0]->as<List>();
      if (! first_row)
        runner::raise_type_error(args[0], List::proto);

      const size_type width = first_row->size();
      const size_type height = args.size();
      value_.resize(height, width, false);
      for (unsigned i = 0; i < height; ++i)
      {
        if (rList l = args[i]->as<List>())
        {
          if (width != l->size())
            FRAISE("expecting rows of size %d, got size %d for row %d",
                   width, l->size(), i + 1);
          unsigned j = 0;
          foreach(const rObject& o, l->value_get())
          {
            if (rFloat f = o->as<Float>())
              value_(i, j) = f->value_get();
            else
              runner::raise_type_error(args[i], Float::proto);
            ++j;
          }
        }
        else
          runner::raise_type_error(args[i], List::proto);
      }
      return this;
    }

    rMatrix
    Matrix::fromList(const rList& model)
    {
      return fromArgsList(model->value_get());
    }

#define OP(Op, Name, Sym)                                       \
    rMatrix                                                     \
    Matrix::Name(const objects_type& args)                      \
    {                                                           \
      rMatrix self = args[0]->as<Matrix>();                     \
      if (args.size() != 2)                                     \
        runner::raise_arity_error(1, args.size() - 1);          \
      if (rFloat f = args[1]->as<Float>())                      \
        return self->operator Op(f);                            \
      else if (rMatrix m = args[1]->as<Matrix>())               \
        return self->operator Op(m);                            \
      runner::raise_argument_type_error                         \
        (1, args[1], Matrix::proto, to_urbi(SYMBOL(Sym)));      \
    }

    OP(+,  plus,         PLUS)
    OP(-,  minus,        MINUS)
    OP(/,  div,          SLASH)
    OP(*,  times,        STAR)
    OP(+=, plus_assign,  PLUS_EQ)
    OP(-=, minus_assign, MINUS_EQ)
    OP(/=, div_assign,   SLASH_EQ)
    OP(*=, times_assign, STAR_EQ)

#undef OP

#define OP(Op)                                                \
    rMatrix                                                   \
    Matrix::operator Op(const rMatrix& m) const               \
    {                                                         \
      value_type copy(value_);                                \
      copy Op##= m->value_;                                   \
      return new Matrix(copy);                                \
    }                                                         \
                                                              \
    rMatrix                                                   \
    Matrix::operator Op##=(const rMatrix& m)                  \
    {                                                         \
      value_ Op##= m->value_;                                 \
      return this;                                            \
    }

    OP(+)
    OP(-)

#undef OP

#define OP(Name, Op)                                            \
    rMatrix                                                     \
    Matrix::Name(const rVector& rhs) const                      \
    {                                                           \
      const size_t height = size1();                            \
      const size_t width = size2();                             \
      rMatrix res =                                             \
        Matrix::create_zeros(0, height, width)->as<Matrix>();   \
      Matrix::value_type& v = res->value_get();                 \
      const Vector::value_type& v2 = rhs->value_get();          \
      for (unsigned p1 = 0; p1 < height; ++p1)                  \
        for (unsigned i = 0; i < width; ++i)                    \
          v(p1, i) = value_(p1, i) Op v2(p1);                   \
      return res;                                               \
    }

    OP(rowAdd, +)
    OP(rowSub, -)
    OP(rowMul, *)
    OP(rowDiv, /)
#undef OP

    rMatrix
    Matrix::operator /(const rMatrix& rhs) const
    {
      rMatrix inverse = rhs->invert();
      value_type copy = prod(value_, inverse->value_);
      return new Matrix(copy);
    }

    rMatrix
    Matrix::operator /=(const rMatrix& rhs)
    {
      rMatrix inverse = rhs->invert();
      value_type res = prod(value_, inverse->value_);
      value_ = res;
      return this;
    }

#define OP(Op, Type, Fun)                                       \
    rMatrix                                                     \
    Matrix::operator Op(const r##Type& rhs) const               \
    {                                                           \
      value_type copy = Fun(value_, rhs->value_);               \
      return new Matrix(copy);                                  \
    }                                                           \
                                                                \
    rMatrix                                                     \
    Matrix::operator Op##=(const r##Type& rhs)                  \
    {                                                           \
      value_type res = Fun(value_, rhs->value_);                \
      value_ = res;                                             \
      return this;                                              \
    }

    OP(*, Matrix, prod)
    //OP(*, Vector, prod)

#undef OP

#define OP(Op)                                                 \
    rMatrix                                                    \
    Matrix::operator Op(const rFloat& s) const                 \
    {                                                          \
      value_type copy(value_);                                 \
      copy Op##= s->value_get();                               \
      return new Matrix(copy);                                 \
    }                                                          \
                                                               \
    rMatrix                                                    \
    Matrix::operator Op##=(const rFloat& s)                    \
    {                                                          \
      value_ Op##= s->value_get();                             \
      return this;                                             \
    }

    OP(*)
    OP(/)

#undef OP

#define OP(Op)                                                  \
    rMatrix                                                     \
    Matrix::operator Op(const rFloat& s) const                  \
    {                                                           \
      value_type copy(value_);                                  \
      value_type ones =                                         \
        boost::numeric::ublas::scalar_matrix<ufloat>            \
        (size1(), size2(), s->value_get());                     \
      value_type res = copy Op ones;                            \
      return new Matrix(res);                                   \
    }                                                           \
                                                                \
    rMatrix                                                     \
    Matrix::operator Op##=(const rFloat& s)                     \
    {                                                           \
      rMatrix res = static_cast<Matrix*>(operator Op(s).get()); \
      value_ = res->value_;                                     \
      return this;                                              \
    }

    OP(+)
    OP(-)

#undef OP

    URBI_CXX_OBJECT_INIT(Matrix)
    {
      BIND_VARIADIC(MINUS, minus);
      BIND_VARIADIC(MINUS_EQ, minus_assign);
      BIND_VARIADIC(PLUS, plus);
      BIND_VARIADIC(PLUS_EQ, plus_assign);
      BIND_VARIADIC(SLASH, div);
      BIND_VARIADIC(SLASH_EQ, div_assign);
      BIND_VARIADIC(STAR, times);
      BIND_VARIADIC(STAR_EQ, times_assign);

      //BIND(SBL_SBR, operator());
      //BIND(SBL_SBR_EQ, set);
      //BIND(dot_times, dotWiseMult);
      //BIND(solve);
      BIND(appendRow);
      BIND(asPrintable);
      BIND(asString);
      BIND(asTopLevelPrintable);
      BIND(column);
      BIND(createIdentity, create_identity);
      BIND(createOnes, create_ones);
      BIND(createScalars, create_scalars);
      BIND(createZeros, create_zeros);
      BIND(distanceMatrix);
      BIND(distanceToMatrix);
      BIND(get);
      BIND(invert);
      BIND(resize);
      BIND(row);
      BIND(rowAdd);
      BIND(rowDiv);
      BIND(rowMul);
      BIND(rowNorm);
      BIND(rowSub);
      BIND(set);
      BIND(setRow);
      BIND(transpose);
      BIND(uvalueSerialize);
      bind(libport::Symbol( "[]" ), &Matrix::operator());
      bind(libport::Symbol( "[]=" ), &Matrix::set);

      BIND(EQ_EQ, operator==, bool, (const rObject&) const);
      BIND(size, size, rObject, () const);
      BIND(set, fromList, rMatrix, (const rList&));
      slot_set(SYMBOL(init), new Primitive(&init));
    }

    std::string
    Matrix::asString() const
    {
      return make_string('<', '>', "<", ">");
    }

    std::string
    Matrix::asPrintable() const
    {
      return make_string('[', ']', "Matrix([", "])");
    }

    std::string
    Matrix::make_string(char col_lsep, char col_rsep,
                        const std::string row_lsep,
                        const std::string row_rsep) const
    {
      const size_t height = size1();
      const size_t width = size2();

      std::ostringstream s;
      s << row_lsep;
      for (unsigned i = 0; i < height; ++i)
      {
        if (i)
          s << ", ";
        s << col_lsep;
        for (unsigned j = 0; j < width; ++j)
        {
          if (j)
            s << ", ";
          s << value_(i, j);
        }
        s << col_rsep;
      }
      s << row_rsep;
      return s.str();
    }


    std::string
    Matrix::asTopLevelPrintable() const
    {
      const size_t height = size1();
      const size_t width = size2();

      std::ostringstream s;
      s << "Matrix([";

      for (unsigned i = 0; i < height; ++i)
      {
        if (i)
          s << ',';
        s << std::endl;
        s << "  [";
        for (unsigned j = 0; j < width; ++j)
        {
          if (j)
            s << ", ";
          s << value_(i, j);
        }
        s << "]";
      }

      s << "])";

      return s.str();
    }

    ufloat
    Matrix::operator()(int i, int j) const
    {
      return value_(index1(i), index2(j));
    }

    rMatrix
    Matrix::set(int i, int j, ufloat v)
    {
      value_(index1(i), index2(j)) = v;
      return this;
    }

    ufloat
    Matrix::get(int i, int j)
    {
      return value_(index1(i), index2(j));
    }

    rObject
    Matrix::size() const
    {
      CAPTURE_GLOBAL(Pair);
      return Pair->call(SYMBOL(new), new Float(size1()), new Float(size2()));
    }

    rVector
    Matrix::row(int i) const
    {
      return new Vector(boost::numeric::ublas::row(value_, index1(i)));
    }

    rVector
    Matrix::column(int i) const
    {
      return new Vector(boost::numeric::ublas::column(value_, index2(i)));
    }

    rVector
    Matrix::distanceMatrix() const
    {
      const size_t height = size1();
      const size_t width = size2();
      rVector res(new Vector(height * (height-1) / 2));
      unsigned idx = 0;
      for (unsigned p1 = 0; p1<height; ++p1)
        for (unsigned p2 = p1+1; p2 < height; ++p2)
        {
          ufloat v = 0;
          for (unsigned i=0; i<width; ++i)
          {
            ufloat t = value_(p1, i) - value_(p2, i);
            v += t*t;
          }
          res->value_get()[idx++] = sqrt(v);
        }
      return res;
    }

    rVector
    Matrix::distanceToMatrix(rMatrix b) const
    {
      Matrix::value_type& vb = b->value_get();
      const size_t height = size1();
      const size_t width = size2();
      const size_t height2 = vb.size1();
      if (width != vb.size2())
        FRAISE("incompatible matrix sizes: %s, %s", width, vb.size2());
      rVector res(new Vector(height * height2));
      unsigned idx = 0;
      for (unsigned p1 = 0; p1<height; ++p1)
        for (unsigned p2 = 0; p2 < height2; ++p2)
        {
          ufloat v = 0;
          for (unsigned i=0; i<width; ++i)
          {
            ufloat t = value_(p1, i) - vb(p2, i);
            v += t*t;
          }
          res->value_get()[idx++] = sqrt(v);
        }
      return res;
    }

    rVector
    Matrix::rowNorm() const
    {
      const size_t height = size1();
      const size_t width = size2();
      rVector res(new Vector(height));
      for (unsigned p1 = 0; p1<height; ++p1)
      {
        ufloat v = 0;
        for (unsigned i=0; i<width; ++i)
        {
          ufloat t = value_(p1, i);
          v += t*t;
        }
        res->value_get()[p1] = sqrt(v);
      }
      return res;
    }

    rMatrix
    Matrix::setRow(int r, rVector v)
    {
      int j = index1(r);
      index2(v->size()-1); // Check size
      Vector::value_type& val = v->value_get();
      for (unsigned i = 0; i< v->size(); ++i)
        value_(j, i) = val(i);
      return this;
    }

    rMatrix
    Matrix::appendRow(rVector v)
    {
      value_.resize(size1()+1, size2());
      setRow(size1()-1, v);
      return this;
    }

    rMatrix
    Matrix::resize(size_t i, size_t j)
    {
      value_.resize(i, j);
      return this;
    }

    rObject
    Matrix::uvalueSerialize() const
    {
      CAPTURE_GLOBAL(Binary);
      // This is ugly: we cannot go through object-cast as it would give us
      // back the vector. So make the Binary ourselve.
      // This is also a duplicate of Vector::uvalueSerialize.
      rObject o(const_cast<Matrix*>(this));
      urbi::UValue v = ::uvalue_cast(o);
      rObject res = new object::Object();
      res->proto_add(Binary);
      res->slot_set(SYMBOL(keywords),
                    new object::String(v.binary->getMessage()));
      res->slot_set(SYMBOL(data),
                    new object::String
                    (std::string(static_cast<char*>(v.binary->common.data),
				 v.binary->common.size)));
      return res;
    }
  }
}