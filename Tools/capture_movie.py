# capture_movie.py
# ============================================================
# 用 Movie Render Queue 离线渲染 Demo（电影级画质、可重复、不卡实时帧率）
# 适用：要做评审/作品集用的高质量成品，而不是实时录屏。
#
# 运行方式（二选一）：
#   A. 编辑器里 Output Log 切到 Python 控制台，直接粘贴执行本文件内容；
#   B. 命令行：
#      "D:\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" ^
#        "D:\UnrealProjects\Chuandi\Chuandi.uproject" -ExecutePythonScript="D:\UnrealProjects\Chuandi\Tools\capture_movie.py"
#
# 前置条件：
#   1. 启用插件：Movie Render Pipeline（Edit > Plugins，搜索 Movie Render，勾选后重启编辑器）。
#   2. 已建一个 Level Sequence（例：/Game/Chuandi/Cinematics/DemoMaster），
#      里面摆好相机机位 + 你要展示的 Boss 战演出（或直接录 PIE 实机可改用 Take Recorder）。
#   3. 第一次建议先在 Movie Render Queue 面板手动配一次输出设置，下面脚本会往队列里加 Job。
#
# 注意：UE 各版本 Movie Render Python API 类名略有差异；
#       若下面某行报 AttributeError，按报错里的真实类名微调即可（逻辑不变）。
# ============================================================

import unreal

SEQUENCE_PATH = "/Game/Chuandi/Cinematics/DemoMaster"   # 改成你自己的序列路径
OUTPUT_DIR    = "C:/Movies/ChuandiDemo"                  # 输出目录（图片序列）

def render_demo():
    subsystem = unreal.get_editor_subsystem(unreal.MoviePipelineQueueSubsystem)
    if subsystem is None:
        unreal.log_error("[capture_movie] 找不到 MoviePipelineQueueSubsystem，请确认插件已启用")
        return

    queue = subsystem.get_queue()
    job = queue.allocate_new_job(unreal.MoviePipelineExecutorJob)

    sequence = unreal.load_asset(SEQUENCE_PATH)
    if sequence is None:
        unreal.log_error(f"[capture_movie] 找不到序列：{SEQUENCE_PATH}")
        return
    job.sequence = sequence
    job.job_name = "ChuandiDemo"

    # 输出格式：PNG 图片序列（之后用剪映/ffmpeg 合成 mp4 最稳）
    img_out = unreal.MoviePipelineImageSequenceOutput()
    if hasattr(unreal.MoviePipelineImageSequenceOutput, "OutputFormatType"):
        img_out.output_format = unreal.MoviePipelineImageSequenceOutput.OutputFormatType.PNG
    job.set_configuration_setting(img_out)

    # 输出目录 / 文件名
    out_setting = unreal.MoviePipelineOutputSetting()
    out_setting.output_directory = unreal.DirectoryPath(OUTPUT_DIR)
    out_setting.file_name_format = "Frame_{frame_number}"
    job.set_configuration_setting(out_setting)

    unreal.log(f"[capture_movie] 已加入渲染队列：{SEQUENCE_PATH} -> {OUTPUT_DIR}")
    unreal.log("[capture_movie] 在 Movie Render Queue 面板点 Render 即可开始（或在下面反注释自动渲染）")

    # 如需一键自动开始渲染，取消下面注释（方法名以你 UE 5.8 实际为准）：
    # subsystem.render_queue(queue)

render_demo()
