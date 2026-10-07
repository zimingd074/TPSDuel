# APK 版本更新

当前采用手动分发完整 APK 的方式，适合 PC + 手机局域网测试。游戏尚未实现自动下载更新。

## 手机怎样更新

1. 电脑打包新版 APK，将 `Builds/Android/Android_ETC2/TPSDuel-arm64.apk` 发到手机。
2. 手机打开新版 APK，按 Android 的提示选择更新或安装，覆盖已有应用。通常不需要卸载旧版；卸载可能清除应用本地数据。首次通过文件管理器安装时可能需要允许该来源安装应用。
3. 如果手机已连接 USB 并授权调试，在电脑执行：

```powershell
Set-Location D:\TPSDuel
.\tools\InstallAndroid.ps1 -Launch
```

脚本使用 `adb install -r`，不会自动卸载。安装失败时保留错误信息，不要先用卸载解决签名问题。覆盖安装通常保留应用私有数据；未来存档结构变化仍需编写迁移逻辑。目前比分只属于本次对局，并不是持久化存档。

## 每次发行必须保持的条件

| 项目 | 当前值 | 后续做法 |
| --- | --- | --- |
| 包名 | `com.tpsduel.prototype` | 保持一致 |
| 签名 | Android Debug 测试证书 | 同一条测试更新链保持同一密钥 |
| StoreVersion / versionCode | 12 | 下一次改为 13，再下一次改为 14 |
| VersionDisplayName / versionName | 0.5.4 | 后续每次发行递增可见版本 |
| ProjectVersion | 0.5.4 | 与 Windows 和 Android 发布版本一起更新 |

Android 版本配置位于 `Config/DefaultEngine.ini` 的 AndroidRuntimeSettings 段；工程版本位于 `Config/DefaultGame.ini`。旧测试 APK 实际显示 versionCode=1、versionName=1.0，已归档在 `Builds/Releases/0.1.0/Android`，不是新 APK。

旧包公开签名证书 SHA-256 指纹：

`2f61c8fc170819bfed2e68e49a4b9671281665ac7d28c13f8ece20716460949b`

证书指纹不是私钥，不能拿它给 APK 签名。应备份实际使用的 keystore，私钥不要提交到公开代码仓库。当前开发签名通常由本机 `%USERPROFILE%/.android/debug.keystore` 提供；更换电脑或重装环境时不要让新的开发密钥替代原密钥。

正式发布应使用专门保管的发布密钥。普通侧载不能直接用另一个发布证书覆盖已经安装的开发签名包；应在测试数据可丢弃时切换，或单独建立正式应用包名，再从第一版正式发行起一直保管该发布密钥。

## 构建与校验

```powershell
Set-Location D:\TPSDuel
.\tools\Build.ps1 -Action PackageAndroid
.\tools\CheckAndroidUpdate.ps1
```

本次构建会自动对比已归档旧 APK：验证两份 APK 签名有效、包名相同、证书指纹相同、新版 versionCode 更大。报告在 `Saved/AndroidUpdateCheck.json`。后续与指定已发行版本对比：

```powershell
.\tools\CheckAndroidUpdate.ps1 -PreviousAPK 'D:\某个已发行版本\TPSDuel-arm64.apk' -NewAPK 'D:\TPSDuel\Builds\Android\Android_ETC2\TPSDuel-arm64.apk'
```

该校验确认安装包的更新条件，不能替代真实手机上的覆盖安装、启动和联机验收。连接新版房间时 PC 和手机应使用本次配套构建；跨版本兼容没有得到保证。更新前结束当前对局，更新后重新建房。

本轮以已发行的 v0.4.1 APK 为基准，验证 versionCode 6→7 的覆盖更新。最终 v0.4.2 包归档到 `Builds/Releases/0.4.2/Android/TPSDuel-arm64.apk`；下一轮发布时应将脚本中的旧包基准改为 v0.4.2。此前归档包继续保留。

v0.5.0 真实感升级已生成新版 APK，并以 v0.4.2 为基准通过覆盖更新条件校验：versionCode 7→8，包名与签名一致。新版归档在 `Builds/Releases/0.5.0/Android/TPSDuel-arm64.apk`，报告为 `Saved/AndroidUpdateCheck.json`。手机实际覆盖安装尚未验收。

v0.5.1 移动与射击优化已生成新版 APK，以 v0.5.0 为基准校验通过：versionCode 8→9，包名与签名一致。归档在 `Builds/Releases/0.5.1/Android/TPSDuel-arm64.apk`，详见 [v0.5.1 验证报告](历史/验证报告-v0.5.1.md)。

下蹲与静步版本使用 v0.5.3 / versionCode 11，跳过已撤回的模板实验版本 code 10，避免降低已安装实验包的版本号。该版本结果见 [v0.5.3 验证报告](历史/验证报告-v0.5.3.md)。

本轮蹲姿射击与穿模修复使用 v0.5.4 / versionCode 12，构建脚本默认与已归档的 v0.5.3 / code 11 比较。最终更新校验和真机状态见 [本轮验证报告](验证报告.md)。

## 将来是否能像商业游戏一样自动更新

可以增加版本服务，让游戏启动时检查版本、提示下载新 APK，然后交由 Android 安装界面让用户确认。普通侧载应用不能任意静默替换自己。

也可以另做资源补丁机制，把可兼容的地图、纹理等放进可下载的 pak。UE4 C++、Java、SDK、权限等程序变更仍需更新 APK。资源补丁需要下载校验、版本兼容、失败回滚及 PC/手机一致的资源管理；当前小型 1v1 测试先使用完整 APK 覆盖更新。

## 官方参考

- [Android 应用签名](https://developer.android.com/studio/publish/app-signing)：签名身份、密钥保管和应用更新。
- [Android 应用版本管理](https://developer.android.com/studio/publish/versioning)：versionCode 与 versionName。
- [Epic UE4.27 发布签名](https://dev.epicgames.com/documentation/unreal-engine/signing-projects-for-release?application_version=4.27)：UE Android 签名与 StoreVersion 设置。
