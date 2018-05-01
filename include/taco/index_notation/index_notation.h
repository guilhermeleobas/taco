#ifndef TACO_INDEX_NOTATION_H
#define TACO_INDEX_NOTATION_H

#include <ostream>
#include <string>
#include <memory>
#include <vector>
#include <set>
#include <map>

#include "taco/error.h"
#include "taco/util/intrusive_ptr.h"
#include "taco/util/comparable.h"
#include "taco/type.h"

#include "taco/index_notation/index_notation_nodes_abstract.h"

namespace taco {

class Type;
class Dimension;
class Format;
class Schedule;

class IndexVar;
class TensorVar;

class IndexExpr;
class Assignment;
class Access;

struct AccessNode;
struct LiteralNode;
struct ReductionNode;

struct AssignmentNode;
struct ForallNode;
struct WhereNode;
struct MultiNode;
struct SequenceNode;

/// A tensor index expression describes a tensor computation as a scalar
/// expression where tensors are indexed by index variables (`IndexVar`).  The
/// index variables range over the tensor dimensions they index, and the scalar
/// expression is evaluated at every point in the resulting iteration space.
/// Index variables that are not used to index the result/left-hand-side are
/// called summation variables and are summed over. Some examples:
/// ```
/// // Matrix addition
/// A(i,j) = B(i,j) + C(i,j);
///
/// // Tensor addition (order-3 tensors)
/// A(i,j,k) = B(i,j,k) + C(i,j,k);
///
/// // Matrix-vector multiplication
/// a(i) = B(i,j) * c(j);
///
/// // Tensor-vector multiplication (order-3 tensor)
/// A(i,j) = B(i,j,k) * c(k);
///
/// // Matricized tensor times Khatri-Rao product (MTTKRP) from data analytics
/// A(i,j) = B(i,k,l) * C(k,j) * D(l,j);
/// ```
///
/// @see IndexVar Index into index expressions.
/// @see TensorVar Operands of index expressions.
class IndexExpr : public util::IntrusivePtr<const IndexExprNode> {
public:
  IndexExpr() : util::IntrusivePtr<const IndexExprNode>(nullptr) {}
  IndexExpr(const IndexExprNode* n) : util::IntrusivePtr<const IndexExprNode>(n) {}

  /// Construct a scalar tensor access.
  /// ```
  /// A(i,j) = b;
  /// ```
  IndexExpr(TensorVar);

  /// Consturct an integer literal.
  /// ```
  /// A(i,j) = 1;
  /// ```
  IndexExpr(long long);

  /// Consturct an unsigned integer literal.
  /// ```
  /// A(i,j) = 1u;
  /// ```
  IndexExpr(unsigned long long);

  /// Consturct double literal.
  /// ```
  /// A(i,j) = 1.0;
  /// ```
  IndexExpr(double);

  /// Construct complex literal.
  /// ```
  /// A(i,j) = complex(1.0, 1.0);
  /// ```
  IndexExpr(std::complex<double>);

  /// Split the given index variable `old` into two index variables, `left` and
  /// `right`, at this expression.  This operation only has an effect for binary
  /// expressions. The `left` index variable computes the left-hand-side of the
  /// expression and stores the result in a temporary workspace. The `right`
  /// index variable computes the whole expression, substituting the
  /// left-hand-side for the workspace.
  void splitOperator(IndexVar old, IndexVar left, IndexVar right);

  DataType getDataType() const;
  
  /// Returns the schedule of the index expression.
  const Schedule& getSchedule() const;

  /// Visit the index expression's sub-expressions.
  void accept(IndexExprVisitorStrict *) const;

  /// Print the index expression.
  friend std::ostream& operator<<(std::ostream&, const IndexExpr&);
};

/// Compare two index expressions by value.
bool equals(IndexExpr, IndexExpr);

/// Construct and returns an expression that negates this expression.
/// ```
/// A(i,j) = -B(i,j);
/// ```
IndexExpr operator-(const IndexExpr&);

/// Add two index expressions.
/// ```
/// A(i,j) = B(i,j) + C(i,j);
/// ```
IndexExpr operator+(const IndexExpr&, const IndexExpr&);

/// Subtract an index expressions from another.
/// ```
/// A(i,j) = B(i,j) - C(i,j);
/// ```
IndexExpr operator-(const IndexExpr&, const IndexExpr&);

/// Multiply two index expressions.
/// ```
/// A(i,j) = B(i,j) * C(i,j);  // Component-wise multiplication
/// ```
IndexExpr operator*(const IndexExpr&, const IndexExpr&);

/// Divide an index expression by another.
/// ```
/// A(i,j) = B(i,j) / C(i,j);  // Component-wise division
/// ```
IndexExpr operator/(const IndexExpr&, const IndexExpr&);

/// Get all index variables in the expression
std::vector<IndexVar> getIndexVars(const IndexExpr&);

/// Simplify an index expression by setting the zeroed Access expressions to
/// zero and then propagating and removing zeroes.
IndexExpr simplify(const IndexExpr& expr, const std::set<Access>& zeroed);

/// Return true if the index statement is of the given subtype.  The subtypes
/// are Assignment, Forall, Where, Multi, and Sequence.
template <typename SubType> bool isa(IndexExpr);

/// Casts the index statement to the given subtype. Assumes S is a subtype and
/// the subtypes are Assignment, Forall, Where, Multi, and Sequence.
template <typename SubType> SubType to(IndexExpr);


/// An index expression that represents a tensor access, such as `A(i,j))`.
/// Access expressions are returned when calling the overloaded operator() on
/// a `TensorVar`.  Access expressions can also be assigned an expression, which
/// happens when they occur on the left-hand-side of an assignment.
///
/// @see TensorVar Calling `operator()` on a `TensorVar` returns an `Assign`.
class Access : public IndexExpr {
public:
  Access() = default;
  Access(const AccessNode*);
  Access(const TensorVar& tensorVar, const std::vector<IndexVar>& indices={});

  /// Return the Access expression's TensorVar.
  const TensorVar &getTensorVar() const;

  /// Returns the index variables used to index into the Access's TensorVar.
  const std::vector<IndexVar>& getIndexVars() const;

  /// Assign the result of an expression to a left-hand-side tensor access.
  /// ```
  /// a(i) = b(i) * c(i);
  /// ```
  Assignment operator=(const IndexExpr&);

  /// Must override the default Access operator=, otherwise it is a copy.
  Assignment operator=(const Access&);

  /// Must disambiguate TensorVar as it can be implicitly converted to IndexExpr
  /// and AccesExpr.
  Assignment operator=(const TensorVar&);

  /// Accumulate the result of an expression to a left-hand-side tensor access.
  /// ```
  /// a(i) += B(i,j) * c(j);
  /// ```
  Assignment operator+=(const IndexExpr&);

  typedef AccessNode Node;
};


/// A literal index expression is a scalar literal that is embedded in the code.
/// @note In the future we may allow general tensor literals.
class Literal : public IndexExpr {
public:
  Literal() = default;
  Literal(const LiteralNode*);
  template <typename T> Literal(T val);

  /// Returns the literal value.
  template <typename T> T getVal() const;

  typedef LiteralNode Node;
};


/// A reduction over the components indexed by the reduction variable.
class Reduction : public IndexExpr {
public:
  Reduction(const ReductionNode*);
  Reduction(IndexExpr op, IndexVar var, IndexExpr expr);

  typedef ReductionNode Node;
};

/// Create a summation index expression.
Reduction sum(IndexVar i, IndexExpr expr);


/// A an index statement computes a tensor.  The index statements are assignment
/// forall, where, multi, and sequence.
class IndexStmt : public util::IntrusivePtr<const IndexStmtNode> {
public:
  IndexStmt();
  IndexStmt(const IndexStmtNode* n);

  /// Visit the tensor expression
  void accept(IndexNotationVisitorStrict *) const;

  /// Return the free and reduction index variables in the assignment.
  std::vector<IndexVar> getIndexVars() const;

  /// Returns the domains/dimensions of the index variables in the statement.
  /// These are inferred from the dimensions they access.
  std::map<IndexVar,Dimension> getIndexVarDomains();
};

/// Compare two index statments by value.
bool equals(IndexStmt, IndexStmt);

/// Print the index statement.
std::ostream& operator<<(std::ostream&, const IndexStmt&);

/// Return true if the index statement is of the given subtype.  The subtypes
/// are Assignment, Forall, Where, Multi, and Sequence.
template <typename SubType> bool isa(IndexStmt);

/// Casts the index statement to the given subtype. Assumes S is a subtype and
/// the subtypes are Assignment, Forall, Where, Multi, and Sequence.
template <typename SubType> SubType to(IndexStmt);


/// An assignment statement assigns an index expression to the locations in a
/// tensor given by an lhs access expression.
class Assignment : public IndexStmt {
public:
  Assignment() = default;
  Assignment(const AssignmentNode*);

  /// Create an assignment. Can specify an optional operator `op` that turns the
  /// assignment into a compound assignment, e.g. `+=`.
  Assignment(Access lhs, IndexExpr rhs, IndexExpr op = IndexExpr());

  /// Create an assignment. Can specify an optional operator `op` that turns the
  /// assignment into a compound assignment, e.g. `+=`.
  Assignment(TensorVar tensor, std::vector<IndexVar> indices, IndexExpr rhs,
             IndexExpr op = IndexExpr());

  /// Return the assignment's left-hand side.
  Access getLhs() const;

  /// Return the assignment's right-hand side.
  IndexExpr getRhs() const;

  /// Return the assignment compound operator (e.g., `+=`) or an undefined
  /// expression if the assignment is not compound (`=`).
  IndexExpr getOp() const;

  /// Return the free index variables in the assignment, which are those used to
  /// access the left-hand side.
  const std::vector<IndexVar>& getFreeVars() const;

  /// Return the reduction index variables i nthe assign
  std::vector<IndexVar> getReductionVars() const;

  typedef AssignmentNode Node;
};


/// A forall statement binds an index variable to values and evaluates the
/// sub-statement for each of these values.
class Forall : public IndexStmt {
public:
  Forall() = default;
  Forall(const ForallNode*);
  Forall(IndexVar indexVar, IndexStmt stmt);

  IndexVar getIndexVar() const;
  IndexStmt getStmt() const;

  typedef ForallNode Node;
};

/// Create a forall index statement.
Forall forall(IndexVar i, IndexStmt expr);


/// A where statment has a producer statement that binds a tensor variable in
/// the environment of a consumer statement.
class Where : public IndexStmt {
public:
  Where() = default;
  Where(const WhereNode*);
  Where(IndexStmt consumer, IndexStmt producer);

  IndexStmt getConsumer();
  IndexStmt getProducer();

  typedef WhereNode Node;
};

/// Create a where index statement.
Where where(IndexStmt consumer, IndexStmt producer);


/// A multi statement has two statements that are executed separately, and let
/// us compute more than one tensor in a concrete index notation statement.
class Multi : public IndexStmt {
public:
  Multi() = default;
  Multi(const MultiNode*);
  Multi(IndexStmt stmt1, IndexStmt stmt2);

  IndexStmt getStmt1() const;
  IndexStmt getStmt2() const;

  typedef MultiNode Node;
};

/// Create a multi index statement.
Multi multi(IndexStmt stmt1, IndexStmt stmt2);


/// A sequence statement has two statements, a definition and a mutation, that
/// are executed in sequence.  The defintion creates an index variable and the
/// mutation updates it.
class Sequence : public IndexStmt {
public:
  Sequence() = default;
  Sequence(const SequenceNode*);
  Sequence(IndexStmt definition, IndexStmt mutation);

  IndexStmt getDefinition() const;
  IndexStmt getMutation() const;

  typedef SequenceNode Node;
};

/// Create a sequence index statement.
Sequence sequence(IndexStmt definition, IndexStmt mutation);


/// Index variables are used to index into tensors in index expressions, and
/// they represent iteration over the tensor modes they index into.
class IndexVar : public util::Comparable<IndexVar> {
public:
  IndexVar();
  IndexVar(const std::string& name);

  /// Returns the name of the index variable.
  std::string getName() const;

  friend bool operator==(const IndexVar&, const IndexVar&);
  friend bool operator<(const IndexVar&, const IndexVar&);

private:
  struct Content;
  std::shared_ptr<Content> content;
};

std::ostream& operator<<(std::ostream&, const IndexVar&);


/// A tensor variable in an index expression, which can either be an operand
/// or the result of the expression.
class TensorVar : public util::Comparable<TensorVar> {
public:
  TensorVar();
  TensorVar(const Type& type);
  TensorVar(const std::string& name, const Type& type);
  TensorVar(const Type& type, const Format& format);
  TensorVar(const std::string& name, const Type& type, const Format& format);

  /// Returns the name of the tensor variable.
  std::string getName() const;

  /// Returns the order of the tensor (number of modes).
  int getOrder() const;

  /// Returns the type of the tensor variable.
  const Type& getType() const;

  /// Returns the format of the tensor variable.
  const Format& getFormat() const;

  /// Returns the last assignment to this tensor variable.
  const Assignment& getAssignment() const;

  /// Returns the schedule of the tensor var, which describes how to compile
  /// and execute it's expression.
  const Schedule& getSchedule() const;

  /// Set the name of the tensor variable.
  void setName(std::string name);

  /// Set the index assignment statement that computes the tensor's values.
  void setAssignment(Assignment assignment);

  /// Create an index expression that accesses (reads) this tensor.
  const Access operator()(const std::vector<IndexVar>& indices) const;

  /// Create an index expression that accesses (reads) this tensor.
  template <typename... IndexVars>
  const Access operator()(const IndexVars&... indices) const {
    return static_cast<const TensorVar*>(this)->operator()({indices...});
  }

  /// Create an index expression that accesses (reads or writes) this tensor.
  Access operator()(const std::vector<IndexVar>& indices);

  /// Create an index expression that accesses (reads or writes) this tensor.
  template <typename... IndexVars>
  Access operator()(const IndexVars&... indices) {
    return this->operator()({indices...});
  }

  /// Assign an expression to a scalar tensor.
  Assignment operator=(const IndexExpr&);

  /// Add an expression to a scalar tensor.
  Assignment operator+=(const IndexExpr&);

  friend bool operator==(const TensorVar&, const TensorVar&);
  friend bool operator<(const TensorVar&, const TensorVar&);

private:
  struct Content;
  std::shared_ptr<Content> content;
};

std::ostream& operator<<(std::ostream&, const TensorVar&);


/// Check whether the statment is in the einsum index notation dialect.
/// This means the statement is an assignment, does not have any reduction
/// nodes, and is a sum of product, e.g., `a*...*b + ... + c*...*d`.
bool isEinsumNotation(const IndexStmt&);

/// Check whether the statement is in the reduction index notation dialect.
/// This means the statement is an assignment and that every reduction variable
/// has a reduction node nested above all variable uses.
bool isReductionNotation(const IndexStmt&);

/// Check whether the statement is in the concrete index notation dialect.
/// This means every index variable has a forall node, there are no reduction
/// nodes, and that every reduction variable use is nested inside a compound
/// assignment statement.
bool isConcreteNotation(const IndexStmt&);

/// Convert einsum notation to reduction notation, by applying Einstein's
/// summation convention to sum non-free/reduction variables over their term.
Assignment makeReductionNotation(const Assignment&);
IndexStmt makeReductionNotation(const IndexStmt&);

/// Convert einsum or reduction notation to concrete notation, by inserting
/// forall nodes, replacing reduction nodes by compound assingments, and
/// inserting temporaries as needed.
Assignment makeConcreteNotation(const Assignment&);
IndexStmt makeConcreteNotation(const IndexStmt&);

}
#endif
