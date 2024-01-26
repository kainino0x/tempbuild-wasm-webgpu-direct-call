// Copyright 2021 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.

#include <webgpu/webgpu_cpp.h>

#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <memory>

#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>

static const wgpu::Instance instance = wgpuCreateInstance(nullptr);

void GetDevice(void (*callback)(wgpu::Device)) {
    instance.RequestAdapter(nullptr, [](WGPURequestAdapterStatus status, WGPUAdapter cAdapter, const char* message, void* userdata) {
        if (message) {
            printf("RequestAdapter: %s\n", message);
        }
        assert(status == WGPURequestAdapterStatus_Success);

        wgpu::Adapter adapter = wgpu::Adapter::Acquire(cAdapter);
        adapter.RequestDevice(nullptr, [](WGPURequestDeviceStatus status, WGPUDevice cDevice, const char* message, void* userdata) {
            if (message) {
                printf("RequestDevice: %s\n", message);
            }
            assert(status == WGPURequestDeviceStatus_Success);

            wgpu::Device device = wgpu::Device::Acquire(cDevice);
            reinterpret_cast<void (*)(wgpu::Device)>(userdata)(device);
        }, userdata);
    }, reinterpret_cast<void*>(callback));
}

static const char shaderCode[] = R"(
    @vertex
    fn main_v(@builtin(vertex_index) idx: u32) -> @builtin(position) vec4<f32> {
        var pos = array<vec2<f32>, 3>(
            vec2<f32>(0.0, 0.5), vec2<f32>(-0.5, -0.5), vec2<f32>(0.5, -0.5));
        return vec4<f32>(pos[idx], 0.0, 1.0);
    }

    @fragment
    fn main_f() -> @location(0) vec4<f32> {
        return vec4<f32>(0.0, 0.502, 1.0, 1.0); // 0x80/0xff ~= 0.502
    }
)";

static wgpu::Device device;
static wgpu::Queue queue;
static wgpu::RenderPipeline pipeline;

void init() {
    device.SetUncapturedErrorCallback(
        [](WGPUErrorType errorType, const char* message, void*) {
            printf("%d: %s\n", errorType, message);
        }, nullptr);

    queue = device.GetQueue();

    wgpu::ShaderModule shaderModule{};
    {
        wgpu::ShaderModuleWGSLDescriptor wgslDesc{};
        wgslDesc.code = shaderCode;

        wgpu::ShaderModuleDescriptor descriptor{};
        descriptor.nextInChain = &wgslDesc;
        shaderModule = device.CreateShaderModule(&descriptor);
        shaderModule.GetCompilationInfo([](WGPUCompilationInfoRequestStatus status, const WGPUCompilationInfo* info, void*) {
            assert(status == WGPUCompilationInfoRequestStatus_Success);
            assert(info->messageCount == 0);
            std::printf("Shader compile succeeded\n");
        }, nullptr);
    }

    {
        wgpu::BindGroupLayoutDescriptor bglDesc{};
        auto bgl = device.CreateBindGroupLayout(&bglDesc);
        wgpu::BindGroupDescriptor desc{};
        desc.layout = bgl;
        desc.entryCount = 0;
        desc.entries = nullptr;
        device.CreateBindGroup(&desc);
    }

    {
        wgpu::PipelineLayoutDescriptor pl{};
        pl.bindGroupLayoutCount = 0;
        pl.bindGroupLayouts = nullptr;

        wgpu::ColorTargetState colorTargetState{};
        colorTargetState.format = wgpu::TextureFormat::BGRA8Unorm;

        wgpu::FragmentState fragmentState{};
        fragmentState.module = shaderModule;
        fragmentState.targetCount = 1;
        fragmentState.targets = &colorTargetState;

        wgpu::RenderPipelineDescriptor descriptor{};
        descriptor.layout = device.CreatePipelineLayout(&pl);
        descriptor.vertex.module = shaderModule;
        descriptor.fragment = &fragmentState;
        descriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
        pipeline = device.CreateRenderPipeline(&descriptor);
    }
}

void render(wgpu::TextureView view) {
    wgpu::RenderPassColorAttachment attachment{};
    attachment.view = view;
    attachment.loadOp = wgpu::LoadOp::Clear;
    attachment.storeOp = wgpu::StoreOp::Store;
    attachment.clearValue = {0, 0, 0, 1};

    wgpu::RenderPassDescriptor renderpass{};
    renderpass.colorAttachmentCount = 1;
    renderpass.colorAttachments = &attachment;

    wgpu::CommandBuffer commands;
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        {
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderpass);
            WGPURenderPassEncoder cPass = pass.Get();
            printf("pass id = %zu\n", reinterpret_cast<uintptr_t>(cPass));

            auto t0 = std::chrono::high_resolution_clock::now();

            static constexpr uint32_t kIterationCount = 10'000'000;
#if BENCH_MODE_NoOp_NoJS
            static constexpr char kDescription[] = "NoOp_NoJS";
            for (uint32_t i = 0; i < kIterationCount; ++i) {
                wgpuRenderPassEncoderNoOp_NoJS(cPass, 1);
            }
#elif BENCH_MODE_NoOp_JSByExternref
            static constexpr char kDescription[] = "NoOp_JSByExternref";
            for (uint32_t i = 0; i < kIterationCount; ++i) {
                wgpuRenderPassEncoderNoOp_JSByExternref(cPass, 1);
            }
#elif BENCH_MODE_NoOp_JSByIndex
            static constexpr char kDescription[] = "NoOp_JSByIndex";
            for (uint32_t i = 0; i < kIterationCount; ++i) {
                wgpuRenderPassEncoderNoOp_JSByIndex(cPass, 1);
            }
#elif BENCH_MODE_MultiNoOp_LoopInWasmManyLookup_NoJS
            static constexpr char kDescription[] = "MultiNoOp_LoopInWasmManyLookup_NoJS";
            wgpuRenderPassEncoderMultiNoOp_LoopInWasmManyLookup_NoJS(cPass, kIterationCount, 1);
#elif BENCH_MODE_MultiNoOp_LoopInWasmManyLookup_JSByExternref
            static constexpr char kDescription[] = "MultiNoOp_LoopInWasmManyLookup_JSByExternref";
            wgpuRenderPassEncoderMultiNoOp_LoopInWasmManyLookup_JSByExternref(cPass, kIterationCount, 1);
#elif BENCH_MODE_MultiNoOp_LoopInWasmSingleLookup_NoJS
            static constexpr char kDescription[] = "MultiNoOp_LoopInWasmSingleLookup_NoJS";
            wgpuRenderPassEncoderMultiNoOp_LoopInWasmSingleLookup_NoJS(cPass, kIterationCount, 1);
#elif BENCH_MODE_MultiNoOp_LoopInWasmSingleLookup_JSByExternref
            static constexpr char kDescription[] = "MultiNoOp_LoopInWasmSingleLookup_JSByExternref";
            wgpuRenderPassEncoderMultiNoOp_LoopInWasmSingleLookup_JSByExternref(cPass, kIterationCount, 1);
#elif BENCH_MODE_MultiNoOp_LoopInJS_JSByExternref
            static constexpr char kDescription[] = "MultiNoOp_LoopInJS_JSByExternref";
            wgpuRenderPassEncoderMultiNoOp_LoopInJS_JSByExternref(cPass, kIterationCount, 1);
#elif BENCH_MODE_DRAW
            static constexpr char kDescription[] = "Draw";
            pass.SetPipeline(pipeline);
            for (uint32_t i = 0; i < kIterationCount; ++i) {
                pass.Draw(0);
            }
#elif BENCH_MODE_SET_DRAW
            static constexpr char kDescription[] = "SetPipeline+Draw";
            for (uint32_t i = 0; i < kIterationCount; ++i) {
                pass.SetPipeline(pipeline);
                pass.Draw(0);
            }
#else
#    error "Bench mode not set"
#endif

            auto t1 = std::chrono::high_resolution_clock::now();
            printf("duration of %d %s iterations: %lldms\n",
                kIterationCount, kDescription,
                std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());

            pass.End(); // This prints the noOpAccumulator just to make sure it's not dead code
        }
        commands = encoder.Finish();
    }

    queue.Submit(1, &commands);
}

wgpu::SwapChain swapChain;
const uint32_t kWidth = 300;
const uint32_t kHeight = 150;

void frame() {
    wgpu::TextureView backbuffer = swapChain.GetCurrentTextureView();
    render(backbuffer);
}

void run() {
    init();
    {
        wgpu::SurfaceDescriptorFromCanvasHTMLSelector canvasDesc{};
        canvasDesc.selector = "#canvas";

        wgpu::SurfaceDescriptor surfDesc{};
        surfDesc.nextInChain = &canvasDesc;
        wgpu::Surface surface = instance.CreateSurface(&surfDesc);

        wgpu::SwapChainDescriptor scDesc{};
        scDesc.usage = wgpu::TextureUsage::RenderAttachment;
        scDesc.format = wgpu::TextureFormat::BGRA8Unorm;
        scDesc.width = kWidth;
        scDesc.height = kHeight;
        scDesc.presentMode = wgpu::PresentMode::Fifo;
        swapChain = device.CreateSwapChain(surface, &scDesc);
    }
    emscripten_set_main_loop(frame, 0, false);
}

int main() {
    GetDevice([](wgpu::Device dev) {
        device = dev;
        run();
    });
}
