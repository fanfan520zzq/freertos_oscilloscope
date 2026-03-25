%% SPWM 调制仿真 - 对应 STM32 实际参数
%   调制波频率 fm = 100 Hz   (最终想要的正弦波)
%   载波频率 fc = 100 kHz    (STM32 定时器 PWM 频率)
%   低通滤波器截止频率 = 1 kHz (介于 fm 和 fc 之间)
%   通过仿真验证 SPWM 原理及滤波器效果

clear; close all; clc;

%% 参数设置 (与硬件对应)
fm = 100;               % 调制波频率 (Hz) -> 输出正弦波频率
fc = 100e3;             % 载波频率 (Hz) -> PWM 开关频率
fs = 100 * fc;          % 仿真采样频率 (Hz)，远高于载波，保证波形连续
t_end = 0.04;           % 仿真时长 (s)，包含 4 个 100Hz 周期

% 时间向量
t = (0:1/fs:t_end-1/fs)';

%% 1. 生成调制波（正弦波）和载波（三角波）
modulating_signal = sin(2 * pi * fm * t);   % 调制波，幅值 ±1
carrier_signal = sawtooth(2 * pi * fc * t, 0.5); % 三角波，幅值 ±1

%% 2. SPWM 比较：得到脉冲序列
% 规则：调制波 > 载波 时输出高电平 (1)，否则低电平 (0)
spwm = double(modulating_signal > carrier_signal);

%% 3. 低通滤波提取基波
% 设计二阶 Butterworth 低通滤波器，截止频率 1 kHz
fc_lpf = 1000;          % 滤波器截止频率 (Hz)
[b, a] = butter(2, fc_lpf/(fs/2), 'low');
filtered_signal = filter(b, a, spwm);   % 滤波后得到正弦波

%% 4. 绘图
figure('Position', [100, 100, 1200, 800]);

% 子图1：调制波与载波（局部放大，显示 5 个载波周期）
subplot(4,1,1);
plot(t, modulating_signal, 'b-', 'LineWidth', 1.5); hold on;
plot(t, carrier_signal, 'r-', 'LineWidth', 1);
xlim([0, 5/fc]);        % 只显示 5 个载波周期
xlabel('时间 (s)'); ylabel('幅值');
title('调制波（正弦）与载波（三角）');
legend('调制波', '载波');
grid on;

% 子图2：SPWM 脉冲序列（局部放大）
subplot(4,1,2);
plot(t, spwm, 'k-', 'LineWidth', 0.5);
xlim([0, 5/fc]);
xlabel('时间 (s)'); ylabel('幅值');
title('SPWM 脉冲序列');
grid on;

% 子图3：SPWM 信号及其滤波后的正弦波（一个完整周期）
subplot(4,1,3);
plot(t, spwm, 'k-', 'LineWidth', 0.5); hold on;
plot(t, filtered_signal, 'b-', 'LineWidth', 1.5);
xlim([0, 1/fm]);        % 显示一个调制波周期 (0~0.01s)
xlabel('时间 (s)'); ylabel('幅值');
title('SPWM 信号（黑）与低通滤波后恢复的正弦波（蓝）');
legend('SPWM', '恢复正弦波');
grid on;

% 子图4：频谱分析
subplot(4,1,4);
Nfft = 2^nextpow2(length(spwm));
f_axis = (0:Nfft/2-1) * (fs/Nfft);
spwm_fft = abs(fft(spwm, Nfft)) / length(spwm);
spwm_fft = spwm_fft(1:Nfft/2);
filtered_fft = abs(fft(filtered_signal, Nfft)) / length(filtered_signal);
filtered_fft = filtered_fft(1:Nfft/2);

% 只显示到 5 倍载波频率
f_max_plot = 5*fc;
idx = find(f_axis <= f_max_plot);

plot(f_axis(idx), 20*log10(spwm_fft(idx)+eps), 'k-', 'LineWidth', 1); hold on;
plot(f_axis(idx), 20*log10(filtered_fft(idx)+eps), 'b-', 'LineWidth', 1.5);
xlabel('频率 (Hz)'); ylabel('幅值 (dB)');
title('频谱（黑色：SPWM，蓝色：滤波后）');
legend('SPWM', '恢复正弦波');
grid on;

%% 5. 显示结果
fprintf('SPWM 仿真完成。\n');
fprintf('调制波频率 (输出正弦波): %.1f Hz\n', fm);
fprintf('载波频率 (PWM 频率): %.1f kHz\n', fc/1e3);
fprintf('低通滤波器截止频率: %.1f Hz\n', fc_lpf);
fprintf('滤波后正弦波幅值: %.3f (理论最大值 0.5? 因滤波器和调制比影响)\n', max(filtered_signal));