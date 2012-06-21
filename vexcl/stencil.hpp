#ifndef VEXCL_STENCIL_HPP
#define VEXCL_STENCIL_HPP

/*
The MIT License

Copyright (c) 2012 Denis Demidov <ddemidov@ksu.ru>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

/**
 * \file   stencil.hpp
 * \author Denis Demidov <ddemidov@ksu.ru>
 * \brief  Stencil convolution.
 */

#ifdef WIN32
#  pragma warning(push)
#pragma warning (disable : 4244 4267)
#  define NOMINMAX
#endif

#ifndef __CL_ENABLE_EXCEPTIONS
#  define __CL_ENABLE_EXCEPTIONS
#endif

#ifndef WIN32
#  define INITIALIZER_LISTS_AVAILABLE
#endif

#include <vector>
#include <map>
#include <sstream>
#include <cassert>
#include <CL/cl.hpp>
#include <vexcl/util.hpp>
#include <vexcl/vector.hpp>

namespace vex {

/// Stencil.
/**
 * Should be used for stencil convolutions with vex::vectors as in
 * \code
 * void convolve(
 *	    const vex::stencil<double> &s,
 *	    const vex::vector<double>  &x,
 *	    vex::vector<double> &y)
 * {
 *     y = x * s;
 * }
 * \endcode
 * Stencil should be small enough to fit into local memory of all compute
 * devices it resides on.
 */
template <typename T>
class stencil {
    public:
	/// Costructor.
	/**
	 * \param queue  vector of queues. Each queue represents one
	 *               compute device.
	 * \param st     vector holding stencil values.
	 * \param center center of the stencil.
	 */
	stencil(const std::vector<cl::CommandQueue> &queue,
		const std::vector<T> &st, uint center
		)
	    : queue(queue), squeue(queue.size()), dbuf(queue.size()),
	      s(queue.size()), event(queue.size()),
	      fast_kernel(queue.size()), wgs(queue.size())
	{
	    init(st, center);
	}

	/// Costructor.
	/**
	 * \param queue  vector of queues. Each queue represents one
	 *               compute device.
	 * \param begin  iterator to begin of sequence holding stencil data.
	 * \param end    iterator to end of sequence holding stencil data.
	 * \param center center of the stencil.
	 */
	template <class Iterator>
	stencil(const std::vector<cl::CommandQueue> &queue,
		Iterator begin, Iterator end, uint center
		)
	    : queue(queue), squeue(queue.size()), dbuf(queue.size()),
	      s(queue.size()), event(queue.size()),
	      fast_kernel(queue.size()), wgs(queue.size())
	{
	    std::vector<T> st(begin, end);
	    init(st, center);
	}

#ifdef INITIALIZER_LISTS_AVAILABLE
	/// Costructor.
	/**
	 * \param queue  vector of queues. Each queue represents one
	 *               compute device.
	 * \param st     intializer list holding stencil values.
	 * \param center center of the stencil.
	 */
	stencil(const std::vector<cl::CommandQueue> &queue,
		std::initializer_list<T> list, uint center
		)
	    : queue(queue), squeue(queue.size()), dbuf(queue.size()),
	      s(queue.size()), event(queue.size()),
	      fast_kernel(queue.size()), wgs(queue.size())
	{
	    std::vector<T> st(list);
	    init(st, center);
	}
#endif

	/// Convolve stencil with a vector.
	/**
	 * y = alpha * y + beta * conv(x);
	 * \param x input vector.
	 * \param y output vector.
	 */
	void convolve(const vex::vector<T> &x, vex::vector<T> &y,
		T alpha = 0, T beta = 1) const;
    private:
	const std::vector<cl::CommandQueue> &queue;
	std::vector<cl::CommandQueue> squeue;

	mutable std::vector<T>	hbuf;
	std::vector<cl::Buffer> dbuf;
	std::vector<cl::Buffer> s;
	mutable std::vector<cl::Event>  event;

	int lhalo;
	int rhalo;

	void init(const std::vector<T> &data, uint center);

	std::vector<char> fast_kernel;
	std::vector<uint> wgs;

	static std::map<cl_context, bool>	compiled;
	static std::map<cl_context, cl::Kernel> conv_local;
	static std::map<cl_context, cl::Kernel> conv_local_big;
	static std::map<cl_context, cl::Kernel> conv_remote;
	static std::map<cl_context, uint>	wgsize;
};

template <typename T>
std::map<cl_context, bool> stencil<T>::compiled;

template <typename T>
std::map<cl_context, cl::Kernel> stencil<T>::conv_local;

template <typename T>
std::map<cl_context, cl::Kernel> stencil<T>::conv_local_big;

template <typename T>
std::map<cl_context, cl::Kernel> stencil<T>::conv_remote;

template <typename T>
std::map<cl_context, uint> stencil<T>::wgsize;

template <typename T>
struct Conv {
    Conv(const vex::vector<T> &x, const stencil<T> &s) : x(x), s(s) {}

    const vex::vector<T> &x;
    const stencil<T> &s;
};

template <typename T>
Conv<T> operator*(const vex::vector<T> &x, const stencil<T> &s) {
    return Conv<T>(x, s);
}

template <typename T>
Conv<T> operator*(const stencil<T> &s, const vex::vector<T> &x) {
    return x * s;
}

template <typename T>
const vex::vector<T>& vex::vector<T>::operator=(const Conv<T> &cnv) {
    cnv.s.convolve(cnv.x, *this);
    return *this;
}

template <class Expr, typename T>
struct ExConv {
    ExConv(const Expr &expr, const Conv<T> &cnv, T p)
	: expr(expr), cnv(cnv), p(p) {}

    const Expr    &expr;
    const Conv<T> &cnv;
    T p;
};

template <class Expr, typename T>
typename std::enable_if<Expr::is_expr, ExConv<Expr,T>>::type
operator+(const Expr &expr, const Conv<T> &cnv) {
    return ExConv<Expr,T>(expr, cnv, 1);
}

template <class Expr, typename T>
typename std::enable_if<Expr::is_expr, ExConv<Expr,T>>::type
operator-(const Expr &expr, const Conv<T> &cnv) {
    return ExConv<Expr,T>(expr, cnv, -1);
}

template <typename T> template <class Expr>
const vex::vector<T>& vex::vector<T>::operator=(const ExConv<Expr,T> &xc) {
    *this = xc.expr;
    xc.cnv.s.convolve(xc.cnv.x, *this, 1, xc.p);
    return *this;
}


template <typename T>
void stencil<T>::init(const std::vector<T> &data, uint center) {
    assert(queue.size());
    assert(data.size());
    assert(center < data.size());

    lhalo = center;
    rhalo = data.size() - center - 1;

    for (uint d = 0; d < queue.size(); d++) {
	cl::Context context = queue[d].getInfo<CL_QUEUE_CONTEXT>();
	cl::Device  device  = queue[d].getInfo<CL_QUEUE_DEVICE>();

	if (!compiled[context()]) {
	    std::ostringstream source;

	    source << standard_kernel_header <<
		"typedef " << type_name<T>() << " real;\n"
		"kernel void conv_local(\n"
		"    " << type_name<size_t>() << " n,\n"
		"    int lhalo,\n"
		"    int rhalo,\n"
		"    global const real *x,\n"
		"    global const real *s,\n"
		"    global real *y,\n"
		"    real alpha, real beta,\n"
		"    local real *loc_buf\n"
		"    )\n"
		"{\n"
		"    local real *stencil = loc_buf;\n"
		"    local real *xbuf    = loc_buf + lhalo + rhalo + 1 + lhalo;\n"
		"    size_t block_size = get_local_size(0);\n"
		"    long   g_id = get_global_id(0);\n"
		"    int    l_id = get_local_id(0);\n"
		"    if (l_id < lhalo + rhalo + 1)\n"
		"        stencil[l_id] = s[l_id];\n"
		"    stencil += lhalo;\n"
		"    if (g_id < n) {\n"
		"        xbuf[l_id] = x[g_id];\n"
		"        if (l_id < lhalo && g_id >= lhalo)\n"
		"            xbuf[l_id - lhalo] = x[g_id - lhalo];\n"
		"        if (l_id + rhalo < block_size || g_id + rhalo < n)\n"
		"            xbuf[l_id + rhalo] = x[g_id + rhalo];\n"
		"    }\n"
		"    barrier(CLK_LOCAL_MEM_FENCE);\n"
		"    if (g_id < n) {\n"
		"        real sum = 0;\n"
		"        for(int k = -lhalo; k <= rhalo; k++)\n"
		"            if (g_id + k >= 0 && g_id + k < n)\n"
		"                sum += stencil[k] * xbuf[l_id + k];\n"
		"        if (alpha)\n"
		"            y[g_id] = alpha * y[g_id] + beta * sum;\n"
		"        else\n"
		"            y[g_id] = beta * sum;\n"
		"    }\n"
		"}\n"
		"kernel void conv_local_big(\n"
		"    " << type_name<size_t>() << " n,\n"
		"    int lhalo,\n"
		"    int rhalo,\n"
		"    global const real *x,\n"
		"    global const real *s,\n"
		"    global real *y,\n"
		"    real alpha, real beta\n"
		"    )\n"
		"{\n"
		"    long g_id = get_global_id(0);\n"
		"    s += lhalo;\n"
		"    if (g_id < n) {\n"
		"        real sum = 0;\n"
		"        for(int k = -lhalo; k <= rhalo; k++)\n"
		"            if (g_id + k >= 0 && g_id + k < n)\n"
		"                sum += s[k] * x[g_id + k];\n"
		"        if (alpha)\n"
		"            y[g_id] = alpha * y[g_id] + beta * sum;\n"
		"        else\n"
		"            y[g_id] = beta * sum;\n"
		"    }\n"
		"}\n"
		"kernel void conv_remote(\n"
		"    " << type_name<size_t>() << " n,\n"
		"    char has_left,\n"
		"    char has_right,\n"
		"    int lhalo,\n"
		"    int rhalo,\n"
		"    global const real *x,\n"
		"    global const real *h,\n"
		"    global const real *s,\n"
		"    global real *y,\n"
		"    real beta\n"
		"    )\n"
		"{\n"
		"    long g_id = get_global_id(0);\n"
		"    global const real *xl = h + lhalo;\n"
		"    global const real *xr = h + lhalo;\n"
		"    s += lhalo;\n"
		"    if (g_id < lhalo) {\n"
		"        real sum = 0;\n"
		"        for(int k = -lhalo; k < 0; k++)\n"
		"            if (g_id + k < 0)\n"
		"                sum += s[k] * (has_left ? xl[g_id + k] : x[0]);\n"
		"        y[g_id] += beta * sum;\n"
		"    }\n"
		"    if (g_id < rhalo) {\n"
		"        real sum = 0;\n"
		"        for(int k = 1; k <= rhalo; k++)\n"
		"            if (g_id + k - rhalo >= 0)\n"
		"                sum += s[k] * (has_right ? xr[g_id + k - rhalo] : x[n - 1]);\n"
		"        y[n - rhalo + g_id] += beta * sum;\n"
		"    }\n"
		"}\n";

#ifdef VEXCL_SHOW_KERNELS
	    std::cout << source.str() << std::endl;
#endif

	    auto program = build_sources(context, source.str());

	    conv_local    [context()] = cl::Kernel(program, "conv_local");
	    conv_local_big[context()] = cl::Kernel(program, "conv_local_big");
	    conv_remote   [context()] = cl::Kernel(program, "conv_remote");

	    wgsize[context()] = std::min(
		    std::min(
			kernel_workgroup_size(conv_local [context()], device),
			kernel_workgroup_size(conv_remote[context()], device)
			),
		    kernel_workgroup_size(conv_local_big[context()], device)
		    );

	    compiled[context()] = true;
	}

	/* In order to use fast kernel, following conditions are to be met:
	 * 1. stencil.size() < work_group.size()
	 * 2. 2 * stencil.size() + work_group.size() < local_memory.size()
	 */
	wgs[d] = wgsize[context()];

	uint smem = (
		device.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>() -
		conv_local[context()].getWorkGroupInfo<CL_KERNEL_LOCAL_MEM_SIZE>(device)
		) / sizeof(T);

	while (wgs[d] >= data.size() && wgs[d] + 2 * data.size() > smem)
	    wgs[d] /= 2;

	if (wgs[d] < data.size()) {
	    fast_kernel[d] = false;
	    wgs[d] = wgsize[context()];
	} else {
	    fast_kernel[d] = true;
	}

	s[d] = cl::Buffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		data.size() * sizeof(T), const_cast<T*>(data.data()));
    }

    hbuf.resize(queue.size() * (lhalo + rhalo));

    if (lhalo + rhalo > 0) {
	for(uint d = 0; d < queue.size(); d++) {
	    cl::Context context = queue[d].getInfo<CL_QUEUE_CONTEXT>();
	    cl::Device  device  = queue[d].getInfo<CL_QUEUE_DEVICE>();

	    squeue[d] = cl::CommandQueue(context, device);

	    dbuf[d] = cl::Buffer(context, CL_MEM_READ_WRITE, (lhalo + rhalo) * sizeof(T));
	}
    }
}

template <typename T>
void stencil<T>::convolve(const vex::vector<T> &x, vex::vector<T> &y,
	T alpha, T beta
	) const
{
    if ((queue.size() > 1) && (lhalo + rhalo > 0)) {
	for(uint d = 0; d < queue.size(); d++) {
	    if (d > 0 && rhalo > 0) {
		squeue[d].enqueueReadBuffer(
			x(d), CL_FALSE, 0, rhalo * sizeof(T),
			&hbuf[d * (rhalo + lhalo)], 0, &event[d]);
	    }

	    if (d + 1 < queue.size() && lhalo > 0) {
		squeue[d].enqueueReadBuffer(
			x(d), CL_FALSE, (x.part_size(d) - lhalo) * sizeof(T),
			lhalo * sizeof(T), &hbuf[d * (rhalo + lhalo) + rhalo],
			0, &event[d]);
	    }
	}
    }

    for(uint d = 0; d < queue.size(); d++) {
	cl::Context context = queue[d].getInfo<CL_QUEUE_CONTEXT>();

	size_t g_size = alignup(x.part_size(d), wgs[d]);

	cl::Kernel &kernel = fast_kernel[d] ?
	    conv_local[context()] : conv_local_big[context()];

	uint pos = 0;
	kernel.setArg(pos++, x.part_size(d));
	kernel.setArg(pos++, lhalo);
	kernel.setArg(pos++, rhalo);
	kernel.setArg(pos++, x(d));
	kernel.setArg(pos++, s[d]);
	kernel.setArg(pos++, y(d));
	kernel.setArg(pos++, alpha);
	kernel.setArg(pos++, beta);

	if (fast_kernel[d])
	    kernel.setArg(pos++,
		    cl::__local((wgs[d] + 2 * (lhalo + rhalo) + 1) * sizeof(T)));

	queue[d].enqueueNDRangeKernel(kernel, cl::NullRange, g_size, wgs[d]);

    }

    if (lhalo + rhalo > 0) {
	if (queue.size() > 1)
	    for(uint d = 0; d < queue.size(); d++)
		if ((d > 0 && rhalo > 0) || (d + 1 < queue.size() && lhalo > 0))
		    event[d].wait();

	for(uint d = 0; d < queue.size(); d++) {
	    cl::Context context = queue[d].getInfo<CL_QUEUE_CONTEXT>();

	    if (d > 0 && lhalo > 0) {
		queue[d].enqueueWriteBuffer(dbuf[d], CL_FALSE, 0, lhalo * sizeof(T),
			&hbuf[(d - 1) * (rhalo + lhalo) + rhalo]);
	    }

	    if (d + 1 < queue.size() && rhalo > 0) {
		queue[d].enqueueWriteBuffer(dbuf[d], CL_FALSE,
			lhalo * sizeof(T), rhalo * sizeof(T),
			&hbuf[(d + 1) * (rhalo + lhalo)]);
	    }

	    size_t g_size = std::max(lhalo, rhalo);

	    uint prm = 0;
	    conv_remote[context()].setArg(prm++, x.part_size(d));
	    conv_remote[context()].setArg(prm++, d > 0);
	    conv_remote[context()].setArg(prm++, d + 1 < queue.size());
	    conv_remote[context()].setArg(prm++, lhalo);
	    conv_remote[context()].setArg(prm++, rhalo);
	    conv_remote[context()].setArg(prm++, x(d));
	    conv_remote[context()].setArg(prm++, dbuf[d]);
	    conv_remote[context()].setArg(prm++, s[d]);
	    conv_remote[context()].setArg(prm++, y(d));
	    conv_remote[context()].setArg(prm++, beta);

	    queue[d].enqueueNDRangeKernel(conv_remote[context()],
		    cl::NullRange, g_size, cl::NullRange);
	}
    }
}

/// Generalized stencil.
/**
 * This is generalized stencil class. Basically, this is a small dense matrix.
 * Generalized stencil may be used in combination with standard functions, as
 * in
 * \code
 * vex::gstencil<double> S(ctx.queue(), 2, 3, 1, {
 *     1, -1,  0,
 *     0,  1, -1
 * });
 * y = sin(x * S);
 * \endcode
 * This admittedly unintuitive notation corresponds to
 * \f$y_i = \sum_k sin(\sum_j S_{kj} * x_{i+j-c})\f$, where c is center of the
 * stencil.  Please see github wiki for further examples of this class usage.
 */
template <typename T>
class gstencil {
    public:
	/// Constructor.
	/**
	 * \param queue  vector of queues. Each queue represents one
	 *               compute device.
	 * \param rows   number of rows in stencil matrix.
	 * \param cols   number of cols in stencil matrix.
	 * \param center center of the stencil. This corresponds to the center
	 *		 of a row of the stencil.
	 * \param data   values of stencil matrix in row-major order.
	 */
	gstencil(const std::vector<cl::CommandQueue> &queue,
		 uint rows, uint cols, uint center,
		 const std::vector<T> &data)
	    : queue(queue), squeue(queue.size), dbuf(queue.size()),
	      S(queue.size()), event(queue.size()), rows(rows), cols(cols)
	{
	    init(center, data);
	}

	/// Constructor.
	/**
	 * \param queue  vector of queues. Each queue represents one
	 *               compute device.
	 * \param rows   number of rows in stencil matrix.
	 * \param cols   number of cols in stencil matrix.
	 * \param center center of the stencil. This corresponds to the center
	 *		 of a row of the stencil.
	 * \param begin  iterator to begin of sequence holding stencil data.
	 * \param end    iterator to end of sequence holding stencil data.
	 */
	template <class Iterator>
	gstencil(const std::vector<cl::CommandQueue> &queue,
		 uint rows, uint cols, uint center,
		 Iterator begin, Iterator end)
	    : queue(queue), squeue(queue.size()), dbuf(queue.size()),
	      S(queue.size()), event(queue.size()), rows(rows), cols(cols)
	{
	    std::vector<T> v(begin, end);
	    init(center, v);
	}

#ifdef INITIALIZER_LISTS_AVAILABLE
	/// Costructor.
	/**
	 * \param queue  vector of queues. Each queue represents one
	 *               compute device.
	 * \param rows   number of rows in stencil matrix.
	 * \param cols   number of cols in stencil matrix.
	 * \param center center of the stencil. This corresponds to the center
	 *		 of a row of the stencil.
	 * \param data   values of stencil matrix in row-major order.
	 */
	gstencil(const std::vector<cl::CommandQueue> &queue,
		 uint rows, uint cols, uint center,
		 const std::initializer_list<T> &data)
	    : queue(queue), squeue(queue.size), dbuf(queue.size()),
	      S(queue.size()), event(queue.size()), rows(rows), cols(cols)
	{
	    std::vector<T> v(data);
	    init(center, v);
	}
#endif

	/// Convolve stencil with a vector.
	/**
	 * \param x input vector.
	 * \param y output vector.
	 */
	template <class func_name>
	void convolve(const vex::vector<T> &x, vex::vector<T> &y,
		T alpha = 0, T beta = 1) const;
    private:
	const std::vector<cl::CommandQueue> &queue;
	std::vector<cl::CommandQueue> squeue;

	mutable std::vector<T>	hbuf;
	std::vector<cl::Buffer> dbuf;
	std::vector<cl::Buffer> S;
	mutable std::vector<cl::Event>  event;

	int lhalo;
	int rhalo;
	uint rows;
	uint cols;

	void init(uint center, const std::vector<T> &data);

	template <class func>
	struct exdata {
	    static std::map<cl_context, bool>	    compiled;
	    static std::map<cl_context, cl::Kernel> conv_local;
	    static std::map<cl_context, cl::Kernel> conv_remote;
	    static std::map<cl_context, uint>	    wgsize;
	};
};

template <typename T> template<class func>
std::map<cl_context, bool> gstencil<T>::exdata<func>::compiled;

template <typename T> template<class func>
std::map<cl_context, cl::Kernel> gstencil<T>::exdata<func>::conv_local;

template <typename T> template<class func>
std::map<cl_context, cl::Kernel> gstencil<T>::exdata<func>::conv_remote;

template <typename T> template<class func>
std::map<cl_context, uint> gstencil<T>::exdata<func>::wgsize;

template <typename T>
struct GStencilProd {
    GStencilProd(const vex::vector<T> &x, const gstencil<T> &s) : x(x), s(s) {}
    const vex::vector<T> &x;
    const gstencil<T> &s;
};

template <class T>
GStencilProd<T> operator*(const vex::vector<T> &x, const gstencil<T> &s) {
    return GStencilProd<T>(x, s);
}

template <class T>
GStencilProd<T> operator*(const gstencil<T> &s, const vex::vector<T> &x) {
    return x * s;
}

template <class f, typename T>
struct GConv {
    GConv(const GStencilProd<T> &sp) : x(sp.x), s(sp.s) {}

    const vex::vector<T> &x;
    const gstencil<T> &s;
};

#define OVERLOAD_BUILTIN_FOR_GSTENCIL(name) \
template <typename T> \
GConv<name##_name,T> name(const GStencilProd<T> &s) { \
    return GConv<name##_name, T>(s); \
}

OVERLOAD_BUILTIN_FOR_GSTENCIL(acos)
OVERLOAD_BUILTIN_FOR_GSTENCIL(acosh)
OVERLOAD_BUILTIN_FOR_GSTENCIL(acospi)
OVERLOAD_BUILTIN_FOR_GSTENCIL(asin)
OVERLOAD_BUILTIN_FOR_GSTENCIL(asinh)
OVERLOAD_BUILTIN_FOR_GSTENCIL(asinpi)
OVERLOAD_BUILTIN_FOR_GSTENCIL(atan)
OVERLOAD_BUILTIN_FOR_GSTENCIL(atanh)
OVERLOAD_BUILTIN_FOR_GSTENCIL(atanpi)
OVERLOAD_BUILTIN_FOR_GSTENCIL(cbrt)
OVERLOAD_BUILTIN_FOR_GSTENCIL(ceil)
OVERLOAD_BUILTIN_FOR_GSTENCIL(cos)
OVERLOAD_BUILTIN_FOR_GSTENCIL(cosh)
OVERLOAD_BUILTIN_FOR_GSTENCIL(cospi)
OVERLOAD_BUILTIN_FOR_GSTENCIL(erfc)
OVERLOAD_BUILTIN_FOR_GSTENCIL(erf)
OVERLOAD_BUILTIN_FOR_GSTENCIL(exp)
OVERLOAD_BUILTIN_FOR_GSTENCIL(exp2)
OVERLOAD_BUILTIN_FOR_GSTENCIL(exp10)
OVERLOAD_BUILTIN_FOR_GSTENCIL(expm1)
OVERLOAD_BUILTIN_FOR_GSTENCIL(fabs)
OVERLOAD_BUILTIN_FOR_GSTENCIL(floor)
OVERLOAD_BUILTIN_FOR_GSTENCIL(ilogb)
OVERLOAD_BUILTIN_FOR_GSTENCIL(lgamma)
OVERLOAD_BUILTIN_FOR_GSTENCIL(log)
OVERLOAD_BUILTIN_FOR_GSTENCIL(log2)
OVERLOAD_BUILTIN_FOR_GSTENCIL(log10)
OVERLOAD_BUILTIN_FOR_GSTENCIL(log1p)
OVERLOAD_BUILTIN_FOR_GSTENCIL(logb)
OVERLOAD_BUILTIN_FOR_GSTENCIL(nan)
OVERLOAD_BUILTIN_FOR_GSTENCIL(rint)
OVERLOAD_BUILTIN_FOR_GSTENCIL(rootn)
OVERLOAD_BUILTIN_FOR_GSTENCIL(round)
OVERLOAD_BUILTIN_FOR_GSTENCIL(rsqrt)
OVERLOAD_BUILTIN_FOR_GSTENCIL(sin)
OVERLOAD_BUILTIN_FOR_GSTENCIL(sinh)
OVERLOAD_BUILTIN_FOR_GSTENCIL(sinpi)
OVERLOAD_BUILTIN_FOR_GSTENCIL(sqrt)
OVERLOAD_BUILTIN_FOR_GSTENCIL(tan)
OVERLOAD_BUILTIN_FOR_GSTENCIL(tanh)
OVERLOAD_BUILTIN_FOR_GSTENCIL(tanpi)
OVERLOAD_BUILTIN_FOR_GSTENCIL(tgamma)
OVERLOAD_BUILTIN_FOR_GSTENCIL(trunc)

template <typename T> template <class func>
const vex::vector<T>& vex::vector<T>::operator=(const GConv<func, T> &cnv) {
    cnv.s.template convolve<func>(cnv.x, *this);
    return *this;
}

template <class Expr, class func, typename T>
struct ExGConv {
    ExGConv(const Expr &expr, const GConv<func,T> &cnv, T p)
	: expr(expr), cnv(cnv), p(p) {}

    const Expr &expr;
    const GConv<func,T> &cnv;
    T p;
};

template <class Expr, class func, typename T>
typename std::enable_if<Expr::is_expr, ExGConv<Expr,func,T>>::type
operator+(const Expr &expr, const GConv<func,T> &cnv) {
    return ExGConv<Expr,func,T>(expr, cnv, 1);
}

template <class Expr, class func, typename T>
typename std::enable_if<Expr::is_expr, ExGConv<Expr,func,T>>::type
operator-(const Expr &expr, const GConv<func,T> &cnv) {
    return ExGConv<Expr,func,T>(expr, cnv, -1);
}

template <typename T> template <class Expr, class func>
const vex::vector<T>& vex::vector<T>::operator=(const ExGConv<Expr,func,T> &xc) {
    *this = xc.expr;
    xc.cnv.s.template convolve<func>(xc.cnv.x, *this, 1, xc.p);
    return *this;
}

template <class T>
void gstencil<T>::init(uint center, const std::vector<T> &data) {
    assert(rows && cols);
    assert(rows * cols == data.size());
    assert(center < cols);

    lhalo = center;
    rhalo = cols - center - 1;

    hbuf.resize(queue.size() * (lhalo + rhalo));

    for (uint d = 0; d < queue.size(); d++) {
	cl::Context context = queue[d].getInfo<CL_QUEUE_CONTEXT>();
	cl::Device  device  = queue[d].getInfo<CL_QUEUE_DEVICE>();

	S[d] = cl::Buffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
		data.size() * sizeof(T), const_cast<T*>(data.data()));

	if (lhalo + rhalo > 0) {
	    squeue[d] = cl::CommandQueue(context, device);
	    dbuf[d]   = cl::Buffer(context, CL_MEM_READ_WRITE, (lhalo + rhalo) * sizeof(T));
	}
    }
}

template <class T> template <class func>
void gstencil<T>::convolve(const vex::vector<T> &x, vex::vector<T> &y,
	T alpha, T beta) const
{
    for (uint d = 0; d < queue.size(); d++) {
	cl::Context context = queue[d].getInfo<CL_QUEUE_CONTEXT>();
	cl::Device  device  = queue[d].getInfo<CL_QUEUE_DEVICE>();

	if (!exdata<func>::compiled[context()]) {
	    std::ostringstream source;

	    source << standard_kernel_header <<
		"typedef " << type_name<T>() << " real;\n"
		"kernel void conv_local(\n"
		"    " << type_name<size_t>() << " n,\n"
		"    uint rows, uint cols,\n"
		"    int lhalo, int rhalo,\n"
		"    global const real *x,\n"
		"    global const real *s,\n"
		"    global real *y,\n"
		"    real alpha, real beta,\n"
		"    local real *loc_buf\n"
		"    )\n"
		"{\n"
		"    local real *S = loc_buf;\n"
		"    local real *xbuf = loc_buf + rows * cols + lhalo;\n"
		"    size_t block_size = get_local_size(0);\n"
		"    long   g_id       = get_global_id(0);\n"
		"    int    l_id       = get_local_id(0);\n"
		"    if (l_id < rows * cols)\n"
		"        S[l_id] = s[l_id];\n"
		"    if (g_id < n) {\n"
		"        xbuf[l_id] = x[g_id];\n"
		"        if (l_id < lhalo && g_id >= lhalo)\n"
		"            xbuf[l_id - lhalo] = x[g_id - lhalo];\n"
		"        if (l_id + rhalo < block_size || g_id + rhalo < n)\n"
		"            xbuf[l_id + rhalo] = x[g_id + rhalo];\n"
		"    }\n"
		"    barrier(CLK_LOCAL_MEM_FENCE);\n"
		"    if (g_id >= lhalo && g_id + rhalo < n) {\n"
		"        real srow = 0;\n"
		"        for(int k = 0; k < rows; k++) {\n"
		"            real scol = 0;\n"
		"            for(int j = -lhalo; j <= rhalo; j++)\n"
		"                scol += S[lhalo + j + k * cols] * xbuf[l_id + j];\n"
		"            srow += " << func::value() << "(scol);\n"
		"        }\n"
		"        if (alpha)\n"
		"            y[g_id] = alpha * y[g_id] + beta * srow;\n"
		"        else\n"
		"            y[g_id] = beta * srow;\n"
		"    }\n"
		"}\n"
		"kernel void conv_remote(\n"
		"    " << type_name<size_t>() << " n,\n"
		"    char has_left,\n"
		"    char has_right,\n"
		"    uint rows, uint cols,\n"
		"    int lhalo, int rhalo,\n"
		"    global const real *xloc,\n"
		"    global const real *xrem,\n"
		"    global const real *s,\n"
		"    global real *y,\n"
		"    real alpha, real beta,\n"
		"    local real *xbuf\n"
		"    )\n"
		"{\n"
		"    long g_id = get_global_id(0);\n"
		"    xbuf += lhalo;\n"
		"    xrem += lhalo;\n"
		"    if (g_id < lhalo) {\n"
		"        xbuf[g_id] = xloc[g_id];\n"
		"        xbuf[g_id + rhalo] = xloc[g_id + rhalo];\n"
		"        xbuf[g_id - lhalo] = has_left ? xrem[g_id - lhalo] : xloc[0];\n"
		"    }\n"
		"    barrier(CLK_LOCAL_MEM_FENCE);\n"
		"    if (g_id < lhalo) {\n"
		"        real srow = 0;\n"
		"        for(int k = 0; k < rows; k++) {\n"
		"            real scol = 0;\n"
		"            for(int j = -lhalo; j <= rhalo; j++)\n"
		"                scol += s[lhalo + j + k * cols] * xbuf[g_id + j];\n"
		"            srow += " << func::value() << "(scol);\n"
		"        }\n"
		"        if (alpha)\n"
		"            y[g_id] = alpha * y[g_id] + beta * srow;\n"
		"        else\n"
		"            y[g_id] = beta * srow;\n"
		"    }\n"
		"    barrier(CLK_LOCAL_MEM_FENCE);\n"
		"    if (g_id < rhalo) {\n"
		"        xbuf[g_id] = xloc[n - rhalo + g_id];\n"
		"        xbuf[g_id - lhalo] = xloc[n - rhalo + g_id - lhalo];\n"
		"        xbuf[g_id + rhalo] = has_right ? xrem[g_id] : xloc[n - 1];\n"
		"    }\n"
		"    barrier(CLK_LOCAL_MEM_FENCE);\n"
		"    if (g_id < rhalo) {\n"
		"        real srow = 0;\n"
		"        for(int k = 0; k < rows; k++) {\n"
		"            real scol = 0;\n"
		"            for(int j = -lhalo; j <= rhalo; j++)\n"
		"                scol += s[lhalo + j + k * cols] * xbuf[g_id + j];\n"
		"            srow += sin(scol);\n"
		"        }\n"
		"        if (alpha)\n"
		"            y[n - rhalo + g_id] = alpha * y[n - rhalo + g_id] + beta * srow;\n"
		"        else\n"
		"            y[n - rhalo + g_id] = beta * srow;\n"
		"    }\n"
		"}\n";

#ifdef VEXCL_SHOW_KERNELS
	    std::cout << source.str() << std::endl;
#endif

	    auto program = build_sources(context, source.str());

	    exdata<func>::conv_local [context()] = cl::Kernel(program, "conv_local");
	    exdata<func>::conv_remote[context()] = cl::Kernel(program, "conv_remote");
	    exdata<func>::wgsize[context()] = std::min(
		    kernel_workgroup_size(exdata<func>::conv_local [context()], device),
		    kernel_workgroup_size(exdata<func>::conv_remote[context()], device)
		    );

	    exdata<func>::compiled[context()] = true;
	}
    }

    // Start reading halos.
    if ((queue.size() > 1) && (lhalo + rhalo > 0)) {
	for(uint d = 0; d < queue.size(); d++) {
	    if (d > 0 && rhalo > 0) {
		squeue[d].enqueueReadBuffer(
			x(d), CL_FALSE, 0, rhalo * sizeof(T),
			&hbuf[d * (rhalo + lhalo)], 0, &event[d]);
	    }

	    if (d + 1 < queue.size() && lhalo > 0) {
		squeue[d].enqueueReadBuffer(
			x(d), CL_FALSE, (x.part_size(d) - lhalo) * sizeof(T),
			lhalo * sizeof(T), &hbuf[d * (rhalo + lhalo) + rhalo],
			0, &event[d]);
	    }
	}
    }

    // Apply local kernel.
    for(uint d = 0; d < queue.size(); d++) {
	cl::Context context = queue[d].getInfo<CL_QUEUE_CONTEXT>();

	size_t g_size = alignup(x.part_size(d), exdata<func>::wgsize[context()]);
	size_t l_mem_size = sizeof(T) * (
		exdata<func>::wgsize[context()] + lhalo + rhalo + rows * cols
		);

#ifndef NDEBUG
	cl::Device  device  = queue[d].getInfo<CL_QUEUE_DEVICE>();
	assert(l_mem_size <= 
		device.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>() -
		static_cast<cl::Kernel>(exdata<func>::conv_local[context()]
		    ).getWorkGroupInfo<CL_KERNEL_LOCAL_MEM_SIZE>(device)
	      );
#endif

	uint pos = 0;
	exdata<func>::conv_local[context()].setArg(pos++, x.part_size(d));
	exdata<func>::conv_local[context()].setArg(pos++, rows);
	exdata<func>::conv_local[context()].setArg(pos++, cols);
	exdata<func>::conv_local[context()].setArg(pos++, lhalo);
	exdata<func>::conv_local[context()].setArg(pos++, rhalo);
	exdata<func>::conv_local[context()].setArg(pos++, x(d));
	exdata<func>::conv_local[context()].setArg(pos++, S[d]);
	exdata<func>::conv_local[context()].setArg(pos++, y(d));
	exdata<func>::conv_local[context()].setArg(pos++, alpha);
	exdata<func>::conv_local[context()].setArg(pos++, beta);
	exdata<func>::conv_local[context()].setArg(pos++, cl::__local(l_mem_size));

	queue[d].enqueueNDRangeKernel(exdata<func>::conv_local[context()],
		cl::NullRange, g_size, exdata<func>::wgsize[context()]);

    }

    // Finish reading halos and apply remote kernel.
    if (lhalo + rhalo > 0) {
	if (queue.size() > 1)
	    for(uint d = 0; d < queue.size(); d++)
		if ((d > 0 && rhalo > 0) || (d + 1 < queue.size() && lhalo > 0))
		    event[d].wait();

	for(uint d = 0; d < queue.size(); d++) {
	    cl::Context context = queue[d].getInfo<CL_QUEUE_CONTEXT>();

	    if (d > 0 && lhalo > 0) {
		queue[d].enqueueWriteBuffer(dbuf[d], CL_FALSE, 0, lhalo * sizeof(T),
			&hbuf[(d - 1) * (rhalo + lhalo) + rhalo]);
	    }

	    if (d + 1 < queue.size() && rhalo > 0) {
		queue[d].enqueueWriteBuffer(dbuf[d], CL_FALSE,
			lhalo * sizeof(T), rhalo * sizeof(T),
			&hbuf[(d + 1) * (rhalo + lhalo)]);
	    }

	    size_t g_size = std::max(lhalo, rhalo);
	    auto lmem = cl::__local(sizeof(T) * (exdata<func>::wgsize[context()] + lhalo + rhalo));

	    uint prm = 0;
	    exdata<func>::conv_remote[context()].setArg(prm++, x.part_size(d));
	    exdata<func>::conv_remote[context()].setArg(prm++, d > 0);
	    exdata<func>::conv_remote[context()].setArg(prm++, d + 1 < queue.size());
	    exdata<func>::conv_remote[context()].setArg(prm++, rows);
	    exdata<func>::conv_remote[context()].setArg(prm++, cols);
	    exdata<func>::conv_remote[context()].setArg(prm++, lhalo);
	    exdata<func>::conv_remote[context()].setArg(prm++, rhalo);
	    exdata<func>::conv_remote[context()].setArg(prm++, x(d));
	    exdata<func>::conv_remote[context()].setArg(prm++, dbuf[d]);
	    exdata<func>::conv_remote[context()].setArg(prm++, S[d]);
	    exdata<func>::conv_remote[context()].setArg(prm++, y(d));
	    exdata<func>::conv_remote[context()].setArg(prm++, alpha);
	    exdata<func>::conv_remote[context()].setArg(prm++, beta);
	    exdata<func>::conv_remote[context()].setArg(prm++, lmem);

	    queue[d].enqueueNDRangeKernel(exdata<func>::conv_remote[context()],
		    cl::NullRange, g_size, cl::NullRange);
	}
    }
}

} // namespace vex

#ifdef WIN32
#  pragma warning(pop)
#endif
#endif