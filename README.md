# TPSDuel

UE4.27.2 C++ 的 1v1 第三人称射击局域网原型，工程位于 `D:\TPSDuel`。

支持电脑作为房主、第二个 PC 进程或 Android 手机通过 IP 加入。服务端结算伤害，同步生命值、弹药、死亡重生与比分，先获得 3 次击杀者获胜，支持双方准备后重赛。

- [需求文档](docs/需求文档.md)
- [开发、构建与联机操作](docs/开发与运行.md)
- [本轮验证报告](docs/验证报告.md)
- [手机 APK 更新方式](docs/APK版本更新.md)
- [跑步持枪、射击和动作优化](docs/持枪与输入优化-v0.4.2.md)
- [真实感升级计划](docs/真实感升级计划.md)

打开 `TPSDuel.uproject` 进行开发。PC 双窗口测试执行 `powershell -ExecutionPolicy Bypass -File D:\TPSDuel\tools\RunLocalDuel.ps1`，第二个窗口输入 `127.0.0.1:7777` 加入。

独立 Windows 包在 `Builds/Windows/WindowsNoEditor`，Android 包在 `Builds/Android`。v0.2 增加工业仓库、PBR 材质、UE 自带人物与移动动画，以及竞技 HUD 和自适应手机界面。界面暂用英文；实际构建结果及手机验收状态以验证报告为准。

v0.4.2 修复单人等待区无法开枪和首次鼠标点击被捕获消耗的问题；等待时可以练枪、换弹，正式对局才结算伤害和比分。跑步时斜持枪于胸前，瞄准 / 射击时抬枪，并增加倒退脚步反播和侧步 IK。

v0.5.0 接入 Quantum 完整人物 / 步枪 PBR、真实 HDRI、锈钢与磨损细节、Factory 工业设施，以及 Animation Starter Pack 持枪方向移动和上半身射击 / 换弹；动作使用共享 IK 同步枪、双手与弹匣。Fab 源资产和派生内容保留本地，复现需自行领取资源；具体范围与许可见 [资源来源](docs/资源来源.md)。
