#include "opencl_backend.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace cli {
namespace {
constexpr double pi=3.141592653589793238462643383279502884;
double smooth(double a,double b,double x) {
    double t=std::clamp((x-a)/(b-a),0.,1.);
    return t*t*(3-2*t);
}
double friction(double re,const PolarBatchParameters& p) {
    double t=smooth(p.transition_re*(1-p.transition_width),
                    p.transition_re*(1+p.transition_width),re);
    return (1-t)*1.328/std::sqrt(re)+t*0.074/std::pow(re,0.2);
}
}

std::vector<PolarBatchOutput> evaluate_polar_cpu(const std::vector<PolarBatchInput>& input,
                                                const PolarBatchParameters& p) {
    std::vector<PolarBatchOutput> result;
    result.reserve(input.size());
    for(const auto& item:input) {
        double angle_deg=std::remainder(item.alpha_deg,360.);
        double alpha=angle_deg*pi/180;
        double cl=2*pi*(-alpha-item.a0);
        double delta=p.stall_re_shift_deg*std::log10(item.re/p.transition_re);
        double attached=1-smooth(p.stall_start_deg+delta,p.stall_end_deg+delta,std::abs(angle_deg));
        double sine=std::sin(-alpha);
        double normal=p.normal_pressure*sine*std::abs(sine);
        result.push_back({
            attached*cl+(1-attached)*normal*std::cos(alpha),
            2*friction(item.re,p)+attached*p.viscous_cl2*cl*cl+
                (1-attached)*std::abs(normal*sine),
            -attached*pi*(item.a2-item.a1)/4
        });
    }
    return result;
}

#ifdef _WIN32
namespace {
using cl_int=int;
using cl_uint=unsigned int;
using cl_ulong=std::uint64_t;
using cl_bitfield=cl_ulong;
using cl_device_type=cl_bitfield;
using cl_mem_flags=cl_bitfield;
using cl_context_properties=std::intptr_t;
using cl_bool=cl_uint;
using cl_platform_id=void*;
using cl_device_id=void*;
using cl_context=void*;
using cl_command_queue=void*;
using cl_program=void*;
using cl_kernel=void*;
using cl_mem=void*;
#define CL_CALL __stdcall

constexpr cl_device_type device_type_gpu=1ull<<2;
constexpr cl_uint platform_name_key=0x0902;
constexpr cl_uint device_name_key=0x102b;
constexpr cl_uint device_extensions=0x1030;
constexpr cl_uint device_double_fp_config=0x1032;
constexpr cl_uint program_build_log=0x1183;
constexpr cl_mem_flags mem_read_only=1ull<<2;
constexpr cl_mem_flags mem_write_only=1ull<<1;
constexpr cl_bool true_value=1;

struct Api {
    HMODULE library=nullptr;
    cl_int (CL_CALL *get_platform_ids)(cl_uint,cl_platform_id*,cl_uint*)=nullptr;
    cl_int (CL_CALL *get_platform_info)(cl_platform_id,cl_uint,size_t,void*,size_t*)=nullptr;
    cl_int (CL_CALL *get_device_ids)(cl_platform_id,cl_device_type,cl_uint,cl_device_id*,cl_uint*)=nullptr;
    cl_int (CL_CALL *get_device_info)(cl_device_id,cl_uint,size_t,void*,size_t*)=nullptr;
    cl_context (CL_CALL *create_context)(const cl_context_properties*,cl_uint,const cl_device_id*,
                                         void (CL_CALL *)(const char*,const void*,size_t,void*),void*,cl_int*)=nullptr;
    cl_command_queue (CL_CALL *create_command_queue)(cl_context,cl_device_id,cl_bitfield,cl_int*)=nullptr;
    cl_program (CL_CALL *create_program_with_source)(cl_context,cl_uint,const char**,const size_t*,cl_int*)=nullptr;
    cl_int (CL_CALL *build_program)(cl_program,cl_uint,const cl_device_id*,const char*,
                                    void (CL_CALL *)(cl_program,void*),void*)=nullptr;
    cl_int (CL_CALL *get_program_build_info)(cl_program,cl_device_id,cl_uint,size_t,void*,size_t*)=nullptr;
    cl_kernel (CL_CALL *create_kernel)(cl_program,const char*,cl_int*)=nullptr;
    cl_mem (CL_CALL *create_buffer)(cl_context,cl_mem_flags,size_t,void*,cl_int*)=nullptr;
    cl_int (CL_CALL *set_kernel_arg)(cl_kernel,cl_uint,size_t,const void*)=nullptr;
    cl_int (CL_CALL *enqueue_write_buffer)(cl_command_queue,cl_mem,cl_bool,size_t,size_t,const void*,
                                           cl_uint,const void*,void*)=nullptr;
    cl_int (CL_CALL *enqueue_ndrange_kernel)(cl_command_queue,cl_kernel,cl_uint,const size_t*,
                                             const size_t*,const size_t*,cl_uint,const void*,void*)=nullptr;
    cl_int (CL_CALL *enqueue_read_buffer)(cl_command_queue,cl_mem,cl_bool,size_t,size_t,void*,
                                          cl_uint,const void*,void*)=nullptr;
    cl_int (CL_CALL *release_mem_object)(cl_mem)=nullptr;
    cl_int (CL_CALL *release_kernel)(cl_kernel)=nullptr;
    cl_int (CL_CALL *release_program)(cl_program)=nullptr;
    cl_int (CL_CALL *release_command_queue)(cl_command_queue)=nullptr;
    cl_int (CL_CALL *release_context)(cl_context)=nullptr;

    Api() {
        library=LoadLibraryW(L"OpenCL.dll");
        if(!library)throw std::runtime_error("OpenCL.dll ist nicht verfuegbar");
        try {
#define LOAD(member,name) member=reinterpret_cast<decltype(member)>(GetProcAddress(library,name)); \
        if(!member)throw std::runtime_error(std::string("OpenCL-Funktion fehlt: ")+name)
        LOAD(get_platform_ids,"clGetPlatformIDs");
        LOAD(get_platform_info,"clGetPlatformInfo");
        LOAD(get_device_ids,"clGetDeviceIDs");
        LOAD(get_device_info,"clGetDeviceInfo");
        LOAD(create_context,"clCreateContext");
        LOAD(create_command_queue,"clCreateCommandQueue");
        LOAD(create_program_with_source,"clCreateProgramWithSource");
        LOAD(build_program,"clBuildProgram");
        LOAD(get_program_build_info,"clGetProgramBuildInfo");
        LOAD(create_kernel,"clCreateKernel");
        LOAD(create_buffer,"clCreateBuffer");
        LOAD(set_kernel_arg,"clSetKernelArg");
        LOAD(enqueue_write_buffer,"clEnqueueWriteBuffer");
        LOAD(enqueue_ndrange_kernel,"clEnqueueNDRangeKernel");
        LOAD(enqueue_read_buffer,"clEnqueueReadBuffer");
        LOAD(release_mem_object,"clReleaseMemObject");
        LOAD(release_kernel,"clReleaseKernel");
        LOAD(release_program,"clReleaseProgram");
        LOAD(release_command_queue,"clReleaseCommandQueue");
        LOAD(release_context,"clReleaseContext");
#undef LOAD
        } catch(...) {
            FreeLibrary(library);library=nullptr;
            throw;
        }
    }
    ~Api() {if(library)FreeLibrary(library);}
};

void check(cl_int status,const char* operation) {
    if(status!=0)throw std::runtime_error(std::string(operation)+" fehlgeschlagen (OpenCL "+std::to_string(status)+")");
}
template<class T>
T checked(T value,cl_int status,const char* operation) {
    check(status,operation);
    if(!value)throw std::runtime_error(std::string(operation)+" lieferte kein Objekt");
    return value;
}
std::string platform_text(Api& api,cl_platform_id id,cl_uint key) {
    size_t size=0;check(api.get_platform_info(id,key,0,nullptr,&size),"clGetPlatformInfo");
    std::string value(size,'\0');
    check(api.get_platform_info(id,key,size,value.data(),nullptr),"clGetPlatformInfo");
    if(!value.empty()&&value.back()=='\0')value.pop_back();
    return value;
}
std::string device_text(Api& api,cl_device_id id,cl_uint key) {
    size_t size=0;check(api.get_device_info(id,key,0,nullptr,&size),"clGetDeviceInfo");
    std::string value(size,'\0');
    check(api.get_device_info(id,key,size,value.data(),nullptr),"clGetDeviceInfo");
    if(!value.empty()&&value.back()=='\0')value.pop_back();
    return value;
}
constexpr const char* source=R"CLC(
#pragma OPENCL EXTENSION cl_khr_fp64 : enable
double smoothstep_unit(double a,double b,double x) {
    double t=clamp((x-a)/(b-a),0.0,1.0);
    return t*t*(3.0-2.0*t);
}
__kernel void polar_batch(__global const double* in,__global double* out,uint count,
                          double stall0,double stall1,double shift,double transition,
                          double width,double viscous,double normal_pressure) {
    size_t i=get_global_id(0);
    if(i>=count)return;
    const double pi=3.141592653589793238462643383279502884;
    double a0=in[i],a1=in[count+i],a2=in[2*count+i];
    double alpha_deg=in[3*count+i];
    double alpha=alpha_deg*pi/180.0;
    double re=in[4*count+i];
    double cl=2.0*pi*(-alpha-a0);
    double delta=shift*log10(re/transition);
    double attached=1.0-smoothstep_unit(stall0+delta,stall1+delta,fabs(alpha_deg));
    double sine=sin(-alpha);
    double normal=normal_pressure*sine*fabs(sine);
    double transition_mix=smoothstep_unit(transition*(1.0-width),transition*(1.0+width),re);
    double cf=(1.0-transition_mix)*1.328/sqrt(re)+transition_mix*0.074/pow(re,0.2);
    out[i]=attached*cl+(1.0-attached)*normal*cos(alpha);
    out[count+i]=2.0*cf+attached*viscous*cl*cl+(1.0-attached)*fabs(normal*sine);
    out[2*count+i]=-attached*pi*(a2-a1)/4.0;
}
)CLC";
}

struct OpenClPolarBackend::Impl {
    Api api;
    cl_platform_id platform_id=nullptr;
    cl_device_id device_id=nullptr;
    cl_context context=nullptr;
    cl_command_queue queue=nullptr;
    cl_program program=nullptr;
    cl_kernel kernel=nullptr;
    std::string platform_name,device_name;
    void cleanup() noexcept {
        if(kernel)api.release_kernel(kernel);
        if(program)api.release_program(program);
        if(queue)api.release_command_queue(queue);
        if(context)api.release_context(context);
        kernel=nullptr;program=nullptr;queue=nullptr;context=nullptr;
    }
    ~Impl() {cleanup();}
    Impl() {
        try {
        cl_uint platform_count=0;
        check(api.get_platform_ids(0,nullptr,&platform_count),"clGetPlatformIDs");
        if(platform_count==0)throw std::runtime_error("Keine OpenCL-Plattform gefunden");
        std::vector<cl_platform_id> platforms(platform_count);
        check(api.get_platform_ids(platform_count,platforms.data(),nullptr),"clGetPlatformIDs");
        for(cl_platform_id candidate:platforms) {
            cl_uint device_count=0;
            cl_int status=api.get_device_ids(candidate,device_type_gpu,0,nullptr,&device_count);
            if(status!=0||device_count==0)continue;
            std::vector<cl_device_id> devices(device_count);
            if(api.get_device_ids(candidate,device_type_gpu,device_count,devices.data(),nullptr)!=0)continue;
            for(cl_device_id candidate_device:devices) {
                cl_ulong fp64=0;
                if(api.get_device_info(candidate_device,device_double_fp_config,sizeof(fp64),&fp64,nullptr)!=0||fp64==0)
                    continue;
                std::string extensions=device_text(api,candidate_device,device_extensions);
                if(extensions.find("cl_khr_fp64")==std::string::npos)continue;
                platform_id=candidate;device_id=candidate_device;break;
            }
            if(device_id)break;
        }
        if(!device_id)throw std::runtime_error("Keine OpenCL-GPU mit IEEE-754-double-Unterstuetzung gefunden");
        platform_name=platform_text(api,platform_id,platform_name_key);
        device_name=device_text(api,device_id,device_name_key);
        cl_int status=0;
        context=api.create_context(nullptr,1,&device_id,nullptr,nullptr,&status);
        context=checked(context,status,"clCreateContext");
        queue=api.create_command_queue(context,device_id,0,&status);
        queue=checked(queue,status,"clCreateCommandQueue");
        const char* program_source=source;
        program=api.create_program_with_source(context,1,&program_source,nullptr,&status);
        program=checked(program,status,"clCreateProgramWithSource");
        status=api.build_program(program,1,&device_id,"",nullptr,nullptr);
        if(status!=0) {
            size_t size=0;api.get_program_build_info(program,device_id,program_build_log,0,nullptr,&size);
            std::string log(size,'\0');
            if(size)api.get_program_build_info(program,device_id,program_build_log,size,log.data(),nullptr);
            throw std::runtime_error("OpenCL-Polarkernel konnte nicht gebaut werden: "+log);
        }
        kernel=api.create_kernel(program,"polar_batch",&status);
        kernel=checked(kernel,status,"clCreateKernel");
        } catch(...) {
            cleanup();
            throw;
        }
    }
};

OpenClPolarBackend::OpenClPolarBackend(std::unique_ptr<Impl> impl):impl_(std::move(impl)) {}
OpenClPolarBackend::~OpenClPolarBackend()=default;
std::unique_ptr<OpenClPolarBackend> OpenClPolarBackend::create(std::string& reason) {
    try {return std::unique_ptr<OpenClPolarBackend>(new OpenClPolarBackend(std::make_unique<Impl>()));}
    catch(const std::exception& e){reason=e.what();return nullptr;}
}
const std::string& OpenClPolarBackend::platform() const {return impl_->platform_name;}
const std::string& OpenClPolarBackend::device() const {return impl_->device_name;}
std::vector<PolarBatchOutput> OpenClPolarBackend::evaluate(
    const std::vector<PolarBatchInput>& input,const PolarBatchParameters& p) const {
    if(input.empty())return {};
    if(input.size()>std::numeric_limits<cl_uint>::max())throw std::runtime_error("OpenCL-Polbatch zu gross");
    size_t n=input.size();
    std::vector<double> packed(5*n),output(3*n);
    for(size_t i=0;i<n;++i) {
        packed[i]=input[i].a0;packed[n+i]=input[i].a1;packed[2*n+i]=input[i].a2;
        packed[3*n+i]=std::remainder(input[i].alpha_deg,360.);packed[4*n+i]=input[i].re;
    }
    cl_int status=0;
    cl_mem in=checked(impl_->api.create_buffer(impl_->context,mem_read_only,packed.size()*sizeof(double),nullptr,&status),
                      status,"clCreateBuffer(input)");
    cl_mem out=nullptr;
    try {
        out=checked(impl_->api.create_buffer(impl_->context,mem_write_only,output.size()*sizeof(double),nullptr,&status),
                    status,"clCreateBuffer(output)");
        check(impl_->api.enqueue_write_buffer(impl_->queue,in,true_value,0,packed.size()*sizeof(double),
                                               packed.data(),0,nullptr,nullptr),"clEnqueueWriteBuffer");
        cl_uint count=static_cast<cl_uint>(n);
        auto arg=[&](cl_uint index,size_t size,const void* value) {
            check(impl_->api.set_kernel_arg(impl_->kernel,index,size,value),"clSetKernelArg");
        };
        arg(0,sizeof(in),&in);arg(1,sizeof(out),&out);arg(2,sizeof(count),&count);
        arg(3,sizeof(p.stall_start_deg),&p.stall_start_deg);
        arg(4,sizeof(p.stall_end_deg),&p.stall_end_deg);
        arg(5,sizeof(p.stall_re_shift_deg),&p.stall_re_shift_deg);
        arg(6,sizeof(p.transition_re),&p.transition_re);
        arg(7,sizeof(p.transition_width),&p.transition_width);
        arg(8,sizeof(p.viscous_cl2),&p.viscous_cl2);
        arg(9,sizeof(p.normal_pressure),&p.normal_pressure);
        size_t global=n;
        check(impl_->api.enqueue_ndrange_kernel(impl_->queue,impl_->kernel,1,nullptr,&global,nullptr,0,nullptr,nullptr),
              "clEnqueueNDRangeKernel");
        check(impl_->api.enqueue_read_buffer(impl_->queue,out,true_value,0,output.size()*sizeof(double),
                                              output.data(),0,nullptr,nullptr),"clEnqueueReadBuffer");
    } catch(...) {
        if(out)impl_->api.release_mem_object(out);
        impl_->api.release_mem_object(in);
        throw;
    }
    impl_->api.release_mem_object(out);
    impl_->api.release_mem_object(in);
    std::vector<PolarBatchOutput> result(n);
    for(size_t i=0;i<n;++i)result[i]={output[i],output[n+i],output[2*n+i]};
    return result;
}
#else
struct OpenClPolarBackend::Impl {};
OpenClPolarBackend::OpenClPolarBackend(std::unique_ptr<Impl> impl):impl_(std::move(impl)) {}
OpenClPolarBackend::~OpenClPolarBackend()=default;
std::unique_ptr<OpenClPolarBackend> OpenClPolarBackend::create(std::string& reason) {
    reason="OpenCL-GPU-Backend ist auf dieser Plattform nicht verfuegbar";
    return nullptr;
}
std::vector<PolarBatchOutput> OpenClPolarBackend::evaluate(
    const std::vector<PolarBatchInput>&,const PolarBatchParameters&) const {
    throw std::runtime_error("OpenCL-GPU-Backend ist auf dieser Plattform nicht verfuegbar");
}
const std::string& OpenClPolarBackend::platform() const {static const std::string empty;return empty;}
const std::string& OpenClPolarBackend::device() const {static const std::string empty;return empty;}
#endif
}
