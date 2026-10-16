# 生成 app-B（B 区固件）

app-B 与 app-A 是同一套源码，只差三处，用脚本从 app-A 复制改三处即可：

1. Keil 的 ROM 起始地址：`0x08020000` → `0x08040000`（uvprojx 里的 `<IROM>` 块）
2. scatter 文件：`LR_IROM1/ER_IROM1 0x08020000 0x00020000` → `0x08040000 0x00020000`
3. `app/Include/OTA.h` 里的 `MCU_VER` 改成比 app-A 更高的版本号（例如 2.1.0）

```powershell
# 拷贝一份
Copy-Item -Recurse app-A app-B

# 改链接地址（uvprojx 与 scatter）
$p = 'app-B\MDK-ARM\stm32f407vet6_Finalversion.uvprojx'
(Get-Content $p -Raw) `
  -replace '<StartAddress>0x8020000</StartAddress>','<StartAddress>0x8040000</StartAddress>' `
  | Set-Content $p

$s = 'app-B\MDK-ARM\stm32f407vet6_Finalversion.sct'
(Get-Content $s -Raw) -replace '0x08020000','0x08040000' | Set-Content $s

# 改版本号
(Get-Content 'app-B\app\Include\OTA.h' -Raw) `
  -replace 'MCU_VER "2.0.0"','MCU_VER "2.1.0"' `
  | Set-Content 'app-B\app\Include\OTA.h'
```

编译后把 `app-B\MDK-ARM\...\*.hex` 转成 bin，上传到巴法云对应主题即可。
