#pragma once

// Embedded web UI for the dub siren configuration portal
// Design language: Teenage Engineering / Elektron inspired
namespace web_ui {

static const char* const INDEX_HTML = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
<title>Poorhouse Lane Siren</title>
<style>
@import url('https://fonts.googleapis.com/css2?family=Space+Mono:wght@400;700&display=swap');
*{box-sizing:border-box;margin:0;padding:0}
:root{
  --bg:#0a0a0a;--surface:#141414;--border:#2a2a2a;
  --accent:#ff6600;--accent-dim:#993d00;
  --led-green:#00ff88;--led-off:#1a1a1a;
  --text:#ccc;--text-hi:#fff;--text-lo:#555;
  --danger:#ff3333;--card:var(--surface);
  --font:'Space Mono',monospace;
}

/* Themes — Dark */
[data-theme="midnight"]{--bg:#0a0a0a;--surface:#141414;--border:#2a2a2a;--accent:#ff6600;--accent-dim:#993d00;--led-green:#00ff88;--led-off:#1a1a1a;--text:#ccc;--text-hi:#fff;--text-lo:#555;--danger:#ff3333;--card:#141414}
[data-theme="deep-dub"]{--bg:#05080f;--surface:#0c1220;--border:#1a2744;--accent:#00ccff;--accent-dim:#007799;--led-green:#00ff88;--led-off:#0a1020;--text:#8899bb;--text-hi:#ccddef;--text-lo:#3a4a66;--danger:#ff4466;--card:#0c1220}
[data-theme="reggae"]{--bg:#0a0f06;--surface:#141e0c;--border:#2a3a18;--accent:#ffcc00;--accent-dim:#997a00;--led-green:#00ff44;--led-off:#111a08;--text:#b0c890;--text-hi:#e0f0c0;--text-lo:#4a5a38;--danger:#ff4444;--card:#141e0c}
[data-theme="steppers"]{--bg:#0f0808;--surface:#1a1010;--border:#332020;--accent:#ff2222;--accent-dim:#991414;--led-green:#ff4444;--led-off:#1a0e0e;--text:#cc9999;--text-hi:#ffdddd;--text-lo:#664444;--danger:#ff6644;--card:#1a1010}
[data-theme="roots"]{--bg:#0e0a06;--surface:#1a1408;--border:#302410;--accent:#cc9933;--accent-dim:#8a6622;--led-green:#66cc44;--led-off:#14100a;--text:#aa9070;--text-hi:#ddccaa;--text-lo:#554433;--danger:#cc4422;--card:#1a1408}
[data-theme="irie"]{--bg:#0a060e;--surface:#140e1a;--border:#28183a;--accent:#bb44ff;--accent-dim:#7722aa;--led-green:#44ff88;--led-off:#100a16;--text:#9980bb;--text-hi:#ddccff;--text-lo:#443366;--danger:#ff4466;--card:#140e1a}
[data-theme="kingston"]{--bg:#08080c;--surface:#101018;--border:#22223a;--accent:#ff9900;--accent-dim:#aa6600;--led-green:#00ff66;--led-off:#0c0c14;--text:#9999aa;--text-hi:#ddddee;--text-lo:#444466;--danger:#ff4444;--card:#101018}
[data-theme="dubplate"]{--bg:#0c0c08;--surface:#16160e;--border:#2c2c1c;--accent:#e0c040;--accent-dim:#a08820;--led-green:#88ee44;--led-off:#121210;--text:#aa9;--text-hi:#ddc;--text-lo:#554;--danger:#ee4422;--card:#16160e}
[data-theme="soundsystem"]{--bg:#060a0a;--surface:#0e1616;--border:#1a2c2c;--accent:#00ee99;--accent-dim:#009960;--led-green:#00ffaa;--led-off:#0a1212;--text:#80aaa0;--text-hi:#bbeedc;--text-lo:#3a5550;--danger:#ff5544;--card:#0e1616}
[data-theme="heavyweight"]{--bg:#080608;--surface:#120e12;--border:#261e26;--accent:#ff4488;--accent-dim:#aa2255;--led-green:#44ffaa;--led-off:#100c10;--text:#aa88aa;--text-hi:#eeccee;--text-lo:#553355;--danger:#ff3344;--card:#120e12}
/* Themes — Light */
[data-theme="silver"]{--bg:#f0f0f0;--surface:#ffffff;--border:#d0d0d0;--accent:#ff6600;--accent-dim:#cc5200;--led-green:#00cc66;--led-off:#e0e0e0;--text:#444;--text-hi:#111;--text-lo:#999;--danger:#dd2222;--card:#ffffff}
[data-theme="concrete"]{--bg:#e8e4e0;--surface:#f5f2ee;--border:#c8c4c0;--accent:#2a6b4f;--accent-dim:#1a4a35;--led-green:#2a9b5f;--led-off:#d8d4d0;--text:#4a4640;--text-hi:#1a1816;--text-lo:#9a9690;--danger:#cc3333;--card:#f5f2ee}
[data-theme="driftwood"]{--bg:#ece6dc;--surface:#f8f4ec;--border:#d0c8b8;--accent:#b87333;--accent-dim:#8a5522;--led-green:#55aa44;--led-off:#ddd6c8;--text:#5a5040;--text-hi:#2a2418;--text-lo:#a09880;--danger:#cc4433;--card:#f8f4ec}
[data-theme="overcast"]{--bg:#e4e8ec;--surface:#f0f4f8;--border:#c4c8d0;--accent:#4477cc;--accent-dim:#2a5599;--led-green:#33aa66;--led-off:#d4d8e0;--text:#4a5060;--text-hi:#1a2030;--text-lo:#8a90a0;--danger:#dd3344;--card:#f0f4f8}
[data-theme="bleached"]{--bg:#f4f0e8;--surface:#fffcf4;--border:#d8d0c0;--accent:#cc4400;--accent-dim:#993300;--led-green:#44bb55;--led-off:#e8e0d4;--text:#605848;--text-hi:#302820;--text-lo:#a09080;--danger:#dd2222;--card:#fffcf4}
[data-theme="studio"]{--bg:#eaeaea;--surface:#f8f8f8;--border:#cccccc;--accent:#8833cc;--accent-dim:#6622aa;--led-green:#33cc77;--led-off:#dcdcdc;--text:#505050;--text-hi:#1a1a1a;--text-lo:#909090;--danger:#dd3333;--card:#f8f8f8}

/* Theme Picker */
.theme-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(100px,1fr));gap:8px}
.theme-swatch{position:relative;cursor:pointer;border:2px solid var(--border);padding:10px 8px;text-align:center;transition:all 0.2s}
.theme-swatch:hover{border-color:var(--text-lo)}
.theme-swatch.active{border-color:var(--accent);box-shadow:0 0 0 1px var(--accent)}
.theme-swatch .swatch-bar{height:6px;display:flex;gap:2px;margin-bottom:8px}
.theme-swatch .swatch-bar span{flex:1;display:block}
.theme-swatch .swatch-label{font-size:0.55rem;font-weight:700;letter-spacing:1px;text-transform:uppercase;color:var(--text-lo)}
.theme-swatch.active .swatch-label{color:var(--accent)}

html{font-size:14px}
body{font-family:var(--font);background:var(--bg);color:var(--text);min-height:100vh;-webkit-tap-highlight-color:transparent;-webkit-font-smoothing:antialiased}

.header{display:flex;justify-content:space-between;align-items:center;padding:12px 16px;border-bottom:2px solid var(--accent)}
.header .brand{font-size:0.85rem;font-weight:700;color:var(--text-hi);letter-spacing:3px;text-transform:uppercase}
.header .sub{display:flex;align-items:center;gap:8px;font-size:0.7rem;color:var(--text-lo);letter-spacing:2px;text-transform:uppercase}
.conn-dot{width:8px;height:8px;border-radius:50%;background:var(--led-green);flex-shrink:0;transition:background 0.3s}
.conn-dot.offline{background:var(--danger);animation:blinker 1.5s ease-in-out infinite}
@keyframes blinker{0%,100%{opacity:1}50%{opacity:0.3}}

.offline-banner{display:none;background:var(--danger);color:#fff;text-align:center;padding:8px;font-size:0.7rem;font-weight:700;letter-spacing:2px;text-transform:uppercase}
.offline-banner.show{display:block}

.tabs{display:flex;border-bottom:1px solid var(--border);overflow-x:auto}
.tab{flex:1;padding:10px 4px;text-align:center;cursor:pointer;font-family:var(--font);font-size:0.6rem;font-weight:700;letter-spacing:1px;text-transform:uppercase;color:var(--text-lo);background:var(--bg);border-right:1px solid var(--border);transition:all 0.15s;white-space:nowrap}
.tab:last-child{border-right:none}
.tab.active{color:var(--bg);background:var(--accent)}
.tab:not(.active):hover{color:var(--text)}

.content{padding:16px;margin:0 auto}
.panel{display:none}.panel.active{display:block}
@media(max-width:767px){.content{max-width:540px}}
@media(min-width:768px){.content{max-width:960px}}

.section{font-size:0.65rem;font-weight:700;color:var(--accent);letter-spacing:3px;text-transform:uppercase;padding:12px 0 8px;border-bottom:1px solid var(--border);margin-bottom:8px}

@media(min-width:768px){.banks-row{display:grid;grid-template-columns:1fr 1fr 1fr;gap:16px;margin-bottom:16px}.bank-col .slot-grid{grid-template-columns:1fr 1fr}}
@media(max-width:767px){.banks-row{display:block}.bank-col{margin-bottom:12px}}

.slot-grid{display:grid;grid-template-columns:1fr 1fr;gap:1px;background:var(--border);border:1px solid var(--border);margin-bottom:8px}
.slot{background:var(--surface);padding:10px;position:relative;min-height:100px;display:flex;flex-direction:column;transition:all 0.2s}
.slot.active-slot{border:2px solid var(--led-green)}
.slot.target-slot{border:2px solid var(--accent)}
.target-hint{font-size:0.65rem;color:var(--text-lo);letter-spacing:1px;text-transform:uppercase;padding:8px 0;text-align:center}
.lib-category{font-size:0.6rem;font-weight:700;color:var(--accent);letter-spacing:2px;text-transform:uppercase;padding:10px 0 6px;border-bottom:1px solid var(--border);margin-top:4px}
.lib-preset{display:flex;align-items:center;justify-content:space-between;padding:8px 6px;border-bottom:1px solid var(--border);transition:background 0.15s}
.lib-preset:hover{background:var(--border)}
.lib-preset .lib-name{font-size:0.8rem;color:var(--text-hi);flex:1}
.lib-preset .lib-actions{display:flex;gap:4px}
.lib-preset .lib-btn{font-family:var(--font);font-size:0.55rem;font-weight:700;letter-spacing:1px;text-transform:uppercase;padding:4px 8px;background:var(--bg);color:var(--text-lo);border:1px solid var(--border);cursor:pointer;transition:all 0.15s}
.lib-preset .lib-btn:hover{color:var(--text-hi);border-color:var(--text-lo)}
.lib-preset .lib-btn.primary{background:var(--accent);color:var(--bg);border-color:var(--accent)}
.lib-preset .lib-btn.previewing{background:var(--danger);border-color:var(--danger);color:#fff}
.slot-num{display:flex;align-items:center;gap:6px;margin-bottom:6px}
.slot-num span{font-size:0.65rem;font-weight:700;color:var(--text-lo);letter-spacing:2px}
.led{width:6px;height:6px;border-radius:50%;background:var(--led-off);flex-shrink:0}
.led.active{background:var(--led-green);box-shadow:0 0 6px var(--led-green)}
.slot-name{font-size:0.85rem;font-weight:700;color:var(--text-hi);margin-bottom:2px;word-break:break-word;flex:1}
.slot-cat{font-size:0.55rem;color:var(--text-lo);letter-spacing:1px;text-transform:uppercase;margin-bottom:6px}
.slot-actions{display:flex;gap:3px;margin-top:auto;flex-wrap:wrap}
.slot-btn{font-family:var(--font);font-size:0.55rem;font-weight:700;letter-spacing:1px;text-transform:uppercase;padding:3px 6px;background:var(--bg);color:var(--text-lo);border:1px solid var(--border);cursor:pointer;transition:all 0.15s}
.slot-btn:hover{color:var(--text-hi);border-color:var(--text-lo)}
.slot-btn.active{color:var(--accent);border-color:var(--accent)}
.save-inline{display:none;margin-top:6px}
.save-inline.show{display:flex;gap:4px}
.save-inline input{font-family:var(--font);font-size:0.7rem;background:var(--bg);color:var(--text-hi);border:1px solid var(--accent);padding:3px 6px;flex:1;outline:none;min-width:0}
.save-inline button{font-family:var(--font);font-size:0.55rem;font-weight:700;background:var(--accent);color:var(--bg);border:none;padding:3px 8px;cursor:pointer;letter-spacing:1px}
.preset-loader{background:var(--surface);border:1px solid var(--border);padding:16px;margin-bottom:16px;max-height:60vh;overflow-y:auto}
.preset-loader h3{font-size:0.7rem;font-weight:700;color:var(--accent);letter-spacing:2px;text-transform:uppercase;margin-bottom:12px}

.card{background:var(--surface);border:1px solid var(--border);padding:16px;margin-bottom:8px}
.card h3{font-size:0.7rem;font-weight:700;color:var(--accent);letter-spacing:2px;text-transform:uppercase;margin-bottom:12px}
.form-group{margin-bottom:12px}
.form-group label{display:block;font-size:0.6rem;font-weight:700;color:var(--text-lo);letter-spacing:2px;text-transform:uppercase;margin-bottom:4px}
.form-group input,.form-group select{width:100%;padding:8px;font-family:var(--font);font-size:0.8rem;background:var(--bg);color:var(--text-hi);border:1px solid var(--border);outline:none}
.form-group input:focus,.form-group select:focus{border-color:var(--accent)}
.form-group input[type=range]{padding:4px 0;-webkit-appearance:none;background:transparent}
input[type=range]::-webkit-slider-runnable-track{height:4px;background:var(--border)}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:14px;height:14px;background:var(--accent);margin-top:-5px;cursor:pointer}
input[type=range]::-moz-range-track{height:4px;background:var(--border);border:none}
input[type=range]::-moz-range-thumb{width:14px;height:14px;background:var(--accent);border:none;cursor:pointer}

.seg-ctrl{display:flex;border:1px solid var(--border)}
.seg-btn{flex:1;padding:8px 4px;text-align:center;cursor:pointer;font-family:var(--font);font-size:0.65rem;font-weight:700;letter-spacing:1px;text-transform:uppercase;color:var(--text-lo);background:var(--bg);border-right:1px solid var(--border);transition:all 0.15s}
.seg-btn:last-child{border-right:none}
.seg-btn.active{color:var(--bg);background:var(--accent)}
.seg-btn:not(.active):hover{color:var(--text)}

.toggle-row{display:flex;align-items:center;justify-content:space-between;padding:10px 0;border-bottom:1px solid var(--border)}
.toggle-row span{font-size:0.7rem;letter-spacing:1px;text-transform:uppercase}
.toggle{position:relative;width:36px;height:18px;flex-shrink:0}
.toggle input{opacity:0;width:0;height:0}
.toggle .slider{position:absolute;cursor:pointer;inset:0;background:var(--border);transition:0.2s}
.toggle .slider:before{content:'';position:absolute;height:12px;width:12px;left:3px;bottom:3px;background:var(--text-lo);transition:0.2s}
.toggle input:checked+.slider{background:var(--accent)}
.toggle input:checked+.slider:before{transform:translateX(18px);background:var(--bg)}
.range-val{color:var(--accent);font-weight:700;font-size:0.75rem;min-width:36px;display:inline-block;text-align:right}

.btn{font-family:var(--font);font-size:0.7rem;font-weight:700;letter-spacing:1px;text-transform:uppercase;padding:10px 20px;border:none;cursor:pointer;transition:all 0.15s}
.btn:disabled{opacity:0.5;cursor:not-allowed}
.btn-sm{font-size:0.6rem;padding:8px 12px}
.btn-primary{background:var(--accent);color:var(--bg)}
.btn-primary:hover:not(:disabled){background:var(--accent-dim)}
.btn-secondary{background:var(--bg);color:var(--text);border:1px solid var(--border)}
.btn-secondary:hover:not(:disabled){border-color:var(--text-lo)}
.btn-danger{background:var(--danger);color:var(--bg)}
.btn-danger:hover:not(:disabled){background:#cc2222}
.btn-block{width:100%;display:block}

.network-list{max-height:200px;overflow-y:auto}
.network-item{display:flex;justify-content:space-between;align-items:center;padding:8px 4px;cursor:pointer;border-bottom:1px solid var(--border);font-size:0.8rem}
.network-item:hover{background:var(--border)}
.signal{color:var(--text-lo);font-size:0.7rem}

.toast{position:fixed;bottom:20px;left:50%;transform:translateX(-50%);font-family:var(--font);font-size:0.75rem;font-weight:700;letter-spacing:1px;text-transform:uppercase;background:var(--accent);color:var(--bg);padding:10px 24px;opacity:0;transition:opacity 0.3s;z-index:999}
.toast.show{opacity:1}
.loading{text-align:center;padding:20px;color:var(--text-lo)}
.spinner{display:inline-block;width:16px;height:16px;border:2px solid var(--border);border-top-color:var(--accent);animation:spin 0.8s linear infinite}
@keyframes spin{to{transform:rotate(360deg)}}

.exit-screen{text-align:center;padding:60px 20px}
.exit-screen h2{color:var(--accent);font-size:1rem;letter-spacing:3px;text-transform:uppercase;margin-bottom:8px}
.exit-screen p{color:var(--text-lo);font-size:0.8rem}

/* Live DSP */
.dsp-group{margin-bottom:12px}
.dsp-group h4{font-size:0.6rem;font-weight:700;color:var(--accent);letter-spacing:2px;text-transform:uppercase;margin-bottom:6px;padding-bottom:4px;border-bottom:1px solid var(--border)}
.dsp-slider{display:flex;align-items:center;gap:8px;padding:4px 0}
.dsp-slider label{font-size:0.6rem;font-weight:700;color:var(--text-lo);letter-spacing:1px;text-transform:uppercase;min-width:70px;flex-shrink:0}
.dsp-slider input[type=range]{flex:1}
.dsp-slider .dsp-val{font-size:0.65rem;color:var(--accent);font-weight:700;min-width:55px;text-align:right;flex-shrink:0}

/* Encoder mapping */
.enc-table{width:100%;border-collapse:collapse;font-size:0.7rem}
.enc-table th{font-size:0.6rem;font-weight:700;color:var(--accent);letter-spacing:1px;text-transform:uppercase;padding:6px 4px;border-bottom:1px solid var(--border);text-align:left}
.enc-table td{padding:6px 4px;border-bottom:1px solid var(--border)}
.enc-table select{font-family:var(--font);font-size:0.7rem;background:var(--bg);color:var(--text-hi);border:1px solid var(--border);padding:4px;width:100%}

/* System stats */
.sys-stat{display:flex;justify-content:space-between;padding:8px 0;border-bottom:1px solid var(--border);font-size:0.75rem}
.sys-stat .sys-label{color:var(--text-lo);text-transform:uppercase;letter-spacing:1px;font-size:0.6rem;font-weight:700}
.sys-stat .sys-value{color:var(--text-hi);font-weight:700}
</style>
</head>
<body>
<div class="header">
  <div class="brand">Poorhouse Lane</div>
  <div class="sub"><div class="conn-dot" id="conn-dot"></div> Siren Config</div>
</div>

<div class="offline-banner" id="offline-banner">Connection Lost &mdash; Reconnecting...</div>

<div class="tabs">
  <div class="tab" onclick="showTab('live')">Live</div>
  <div class="tab active" onclick="showTab('presets')">Presets</div>
  <div class="tab" onclick="showTab('encoders')">Encoders</div>
  <div class="tab" onclick="showTab('options')">Options</div>
  <div class="tab" onclick="showTab('wifi')">WiFi</div>
  <div class="tab" onclick="showTab('system')">System</div>
</div>

<div class="content">

<!-- LIVE TAB -->
<div id="live" class="panel">
  <div class="card">
    <h3>Sound Controls</h3>
    <div class="dsp-group">
      <h4>Oscillator</h4>
      <div class="dsp-slider">
        <label>Waveform</label>
        <select id="dsp-waveform" onchange="setDspParam('waveform',this.value)" style="flex:1;font-family:var(--font);font-size:0.75rem;background:var(--bg);color:var(--text-hi);border:1px solid var(--border);padding:4px">
          <option value="0">Sine</option><option value="1">Square</option><option value="2">Saw</option><option value="3">Triangle</option>
        </select>
      </div>
      <div class="dsp-slider">
        <label>Frequency</label>
        <input type="range" id="dsp-freq" min="30" max="8000" value="440" oninput="onDspSlider('freq',this.value)">
        <span class="dsp-val" id="dsp-freq-val">440 Hz</span>
      </div>
    </div>
    <div class="dsp-group">
      <h4>LFO</h4>
      <div class="dsp-slider">
        <label>Shape</label>
        <select id="dsp-lfo-waveform" onchange="setDspParam('lfo_waveform',this.value)" style="flex:1;font-family:var(--font);font-size:0.75rem;background:var(--bg);color:var(--text-hi);border:1px solid var(--border);padding:4px">
          <option value="0">Sine</option><option value="1">Triangle</option><option value="2">Square</option>
          <option value="3">Ramp Up</option><option value="4">Ramp Down</option><option value="5">S&amp;H</option>
          <option value="6">Exp Rise</option><option value="7">Exp Fall</option>
        </select>
      </div>
      <div class="dsp-slider">
        <label>Rate</label>
        <input type="range" id="dsp-lfo_rate" min="0" max="1000" value="35" oninput="onDspSliderLog('lfo_rate',this.value,0.1,20)">
        <span class="dsp-val" id="dsp-lfo_rate-val">0.35 Hz</span>
      </div>
      <div class="dsp-slider">
        <label>Depth</label>
        <input type="range" id="dsp-lfo_depth" min="0" max="100" value="35" oninput="onDspSliderPct('lfo_depth',this.value)">
        <span class="dsp-val" id="dsp-lfo_depth-val">35%</span>
      </div>
    </div>
    <div class="dsp-group">
      <h4>Filter</h4>
      <div class="dsp-slider">
        <label>Cutoff</label>
        <input type="range" id="dsp-filter_cutoff" min="0" max="1000" value="500" oninput="onDspSliderLog('filter_cutoff',this.value,20,20000)">
        <span class="dsp-val" id="dsp-filter_cutoff-val">8000 Hz</span>
      </div>
      <div class="dsp-slider">
        <label>Resonance</label>
        <input type="range" id="dsp-filter_reso" min="0" max="100" value="0" oninput="onDspSliderPct('filter_reso',this.value,0.95)">
        <span class="dsp-val" id="dsp-filter_reso-val">0%</span>
      </div>
    </div>
    <div class="dsp-group">
      <h4>Delay</h4>
      <div class="dsp-slider">
        <label>Time</label>
        <input type="range" id="dsp-delay_time" min="0" max="1000" value="375" oninput="onDspSliderLog('delay_time',this.value,0.001,1.0)">
        <span class="dsp-val" id="dsp-delay_time-val">375 ms</span>
      </div>
      <div class="dsp-slider">
        <label>Feedback</label>
        <input type="range" id="dsp-delay_feedback" min="0" max="100" value="55" oninput="onDspSliderPct('delay_feedback',this.value,0.95)">
        <span class="dsp-val" id="dsp-delay_feedback-val">55%</span>
      </div>
      <div class="dsp-slider">
        <label>Mix</label>
        <input type="range" id="dsp-delay_mix" min="0" max="100" value="30" oninput="onDspSliderPct('delay_mix',this.value)">
        <span class="dsp-val" id="dsp-delay_mix-val">30%</span>
      </div>
    </div>
    <div class="dsp-group">
      <h4>Envelope</h4>
      <div class="dsp-slider">
        <label>Release</label>
        <input type="range" id="dsp-release_time" min="0" max="1000" value="50" oninput="onDspSliderLog('release_time',this.value,0.01,5.0)">
        <span class="dsp-val" id="dsp-release_time-val">50 ms</span>
      </div>
      <div class="dsp-slider">
        <label>Pitch Env</label>
        <div class="seg-ctrl" style="flex:1">
          <div class="seg-btn" onclick="setDspParam('pitch_env',-1);updatePitchEnvUI(-1)">Fall</div>
          <div class="seg-btn active" onclick="setDspParam('pitch_env',0);updatePitchEnvUI(0)">Off</div>
          <div class="seg-btn" onclick="setDspParam('pitch_env',1);updatePitchEnvUI(1)">Rise</div>
        </div>
      </div>
    </div>
    <div class="dsp-group">
      <h4>Reverb</h4>
      <div class="dsp-slider">
        <label>Mix</label>
        <input type="range" id="dsp-reverb_mix" min="0" max="100" value="35" oninput="onDspSliderPct('reverb_mix',this.value)">
        <span class="dsp-val" id="dsp-reverb_mix-val">35%</span>
      </div>
    </div>
    <div style="display:flex;gap:8px;margin-top:12px">
      <button class="btn btn-secondary" onclick="loadDspState()" style="flex:1">Refresh</button>
      <button class="btn btn-danger" onclick="confirmAction('Reset all parameters to factory defaults?',resetDefaults)" style="flex:1">Reset Defaults</button>
    </div>
    <div class="dsp-group" style="margin-top:16px">
      <h4>Save to Preset</h4>
      <div class="dsp-slider">
        <label>Bank</label>
        <select id="live-save-bank" style="flex:1;font-family:var(--font);font-size:0.75rem;background:var(--bg);color:var(--text-hi);border:1px solid var(--border);padding:4px">
          <option value="user">User</option>
          <option value="standard">Standard</option>
          <option value="experimental">Experimental</option>
        </select>
      </div>
      <div class="dsp-slider">
        <label>Slot</label>
        <select id="live-save-slot" style="flex:1;font-family:var(--font);font-size:0.75rem;background:var(--bg);color:var(--text-hi);border:1px solid var(--border);padding:4px">
          <option value="0">1</option><option value="1">2</option>
          <option value="2">3</option><option value="3">4</option>
        </select>
      </div>
      <div class="dsp-slider">
        <label>Name</label>
        <input type="text" id="live-save-name" placeholder="Preset name"
               style="flex:1;font-family:var(--font);font-size:0.75rem;background:var(--bg);color:var(--text-hi);border:1px solid var(--border);padding:4px">
      </div>
      <button class="btn btn-primary" onclick="doLiveSave()" style="width:100%;margin-top:4px">Save Preset</button>
    </div>
  </div>
</div>

<!-- PRESETS TAB -->
<div id="presets" class="panel active">
  <div id="target-hint" class="target-hint">Tap a slot to select it, then pick a preset below</div>
  <div class="banks-row">
    <div class="bank-col">
      <div class="section">User Bank</div>
      <div class="slot-grid" id="slot-grid-user"></div>
    </div>
    <div class="bank-col">
      <div class="section">Standard Bank</div>
      <div class="slot-grid" id="slot-grid-standard"></div>
    </div>
    <div class="bank-col">
      <div class="section">Experimental Bank</div>
      <div class="slot-grid" id="slot-grid-experimental"></div>
    </div>
  </div>
  <div id="library-browser" class="preset-loader" style="display:none">
    <h3>Preset Library <span id="target-label" style="color:var(--text-lo);font-size:0.6rem;letter-spacing:1px"></span></h3>
    <div id="library-list"></div>
  </div>
</div>

<!-- ENCODERS TAB -->
<div id="encoders" class="panel">
  <div class="card">
    <h3>Device Layout</h3>
    <div id="enc-device" style="padding:12px 0;max-width:340px;margin:0 auto">
      <div style="display:flex;justify-content:space-between;margin-bottom:16px">
        <div style="flex:1;text-align:center">
          <div style="width:48px;height:48px;border-radius:50%;border:2px solid var(--accent);margin:0 auto 4px;display:flex;align-items:center;justify-content:center;font-size:0.65rem;font-weight:700;color:var(--accent)">1</div>
          <div style="font-size:0.55rem;color:var(--text-hi);font-weight:700" id="enc-label-a0">Frequency</div>
          <div style="font-size:0.5rem;color:var(--text-lo)" id="enc-label-b0">LFO Depth</div>
        </div>
        <div style="flex:1;text-align:center">
          <div style="width:48px;height:48px;border-radius:50%;border:2px solid var(--accent);margin:0 auto 4px;display:flex;align-items:center;justify-content:center;font-size:0.65rem;font-weight:700;color:var(--accent)">2</div>
          <div style="font-size:0.55rem;color:var(--text-hi);font-weight:700" id="enc-label-a1">LFO Rate</div>
          <div style="font-size:0.5rem;color:var(--text-lo)" id="enc-label-b1">Release</div>
        </div>
        <div style="flex:1;text-align:center">
          <div style="width:48px;height:48px;border-radius:50%;border:2px solid var(--accent);margin:0 auto 4px;display:flex;align-items:center;justify-content:center;font-size:0.65rem;font-weight:700;color:var(--accent)">3</div>
          <div style="font-size:0.55rem;color:var(--text-hi);font-weight:700" id="enc-label-a2">Filter</div>
          <div style="font-size:0.5rem;color:var(--text-lo)" id="enc-label-b2">Resonance</div>
        </div>
      </div>
      <div style="display:flex;justify-content:space-between;margin-bottom:16px">
        <div style="flex:1;text-align:center">
          <div style="width:48px;height:48px;border-radius:50%;border:2px solid var(--accent);margin:0 auto 4px;display:flex;align-items:center;justify-content:center;font-size:0.65rem;font-weight:700;color:var(--accent)">4</div>
          <div style="font-size:0.55rem;color:var(--text-hi);font-weight:700" id="enc-label-a3">Delay Time</div>
          <div style="font-size:0.5rem;color:var(--text-lo)" id="enc-label-b3">Delay Mix</div>
        </div>
        <div style="flex:1;text-align:center">
          <div style="width:48px;height:48px;border-radius:50%;border:2px solid var(--accent);margin:0 auto 4px;display:flex;align-items:center;justify-content:center;font-size:0.65rem;font-weight:700;color:var(--accent)">5</div>
          <div style="font-size:0.55rem;color:var(--text-hi);font-weight:700" id="enc-label-a4">Delay FB</div>
          <div style="font-size:0.5rem;color:var(--text-lo)" id="enc-label-b4">Reverb Mix</div>
        </div>
        <div style="flex:1;text-align:center">
          <div style="display:inline-flex;flex-direction:column;border:2px solid var(--border);border-radius:4px;overflow:hidden;margin:0 auto 4px">
            <div style="padding:4px 10px;font-size:0.45rem;font-weight:700;color:var(--text-lo);border-bottom:1px solid var(--border)">RISE</div>
            <div style="padding:4px 10px;font-size:0.45rem;font-weight:700;color:var(--text-hi);background:var(--border)">OFF</div>
            <div style="padding:4px 10px;font-size:0.45rem;font-weight:700;color:var(--text-lo);border-top:1px solid var(--border)">FALL</div>
          </div>
          <div style="font-size:0.5rem;color:var(--text-lo)">Pitch Env</div>
        </div>
      </div>
      <div style="display:flex;justify-content:space-between;margin-bottom:8px">
        <div style="flex:1;text-align:center">
          <div style="width:44px;height:28px;border:2px solid var(--text-lo);border-radius:4px;margin:0 auto 4px;display:flex;align-items:center;justify-content:center;font-size:0.5rem;font-weight:700;color:var(--text-lo)">PRESET</div>
        </div>
        <div style="flex:1;text-align:center">
          <div style="width:44px;height:28px;border:2px solid var(--text-lo);border-radius:4px;margin:0 auto 4px;display:flex;align-items:center;justify-content:center;font-size:0.5rem;font-weight:700;color:var(--text-lo)">SHIFT</div>
        </div>
        <div style="flex:1;text-align:center">
          <div style="width:44px;height:28px;border:2px solid var(--danger);border-radius:4px;margin:0 auto 4px;display:flex;align-items:center;justify-content:center;font-size:0.5rem;font-weight:700;color:var(--danger)">TRIG</div>
        </div>
      </div>
      <div style="display:flex;justify-content:center;gap:16px;font-size:0.5rem;color:var(--text-lo)">
        <span style="color:var(--text-hi)">A = Bank A</span>
        <span>B = Bank B (Shift)</span>
      </div>
    </div>
  </div>
  <div class="card">
    <h3>Encoder Mapping</h3>
    <table class="enc-table" id="enc-table"><tr><td>Loading...</td></tr></table>
    <div style="display:flex;gap:8px;margin-top:8px">
      <button class="btn btn-primary btn-sm" onclick="saveEncoderMap()">Save Mapping</button>
      <button class="btn btn-secondary btn-sm" onclick="resetEncoderMap()">Reset Default</button>
    </div>
  </div>
  <div class="card">
    <h3>Parameter Sensitivity</h3>
    <p style="font-size:0.6rem;color:var(--text-lo);margin-bottom:8px">Adjust how fast each parameter responds to encoder turns. Lower = finer control, higher = faster sweeps.</p>
    <div id="sensitivity-list">Loading...</div>
    <div style="display:flex;gap:8px;margin-top:8px">
      <button class="btn btn-primary btn-sm" onclick="saveSensitivity()">Save Sensitivity</button>
      <button class="btn btn-secondary btn-sm" onclick="resetSensitivity()">Reset Default</button>
    </div>
  </div>
</div>

<!-- OPTIONS TAB -->
<div id="options" class="panel">
  <div class="card">
    <h3>Signal Chain</h3>
    <div class="dsp-group">
      <h4>Routing</h4>
      <div class="dsp-slider">
        <label>FX Order</label>
        <select id="fx-chain" style="flex:1;font-family:var(--font);font-size:0.75rem;background:var(--bg);color:var(--text-hi);border:1px solid var(--border);padding:4px">
          <option value="0">Filter &rarr; Delay &rarr; Reverb (Default)</option>
          <option value="1">Filter &rarr; Reverb &rarr; Delay</option>
          <option value="2">Delay &rarr; Filter &rarr; Reverb</option>
          <option value="3">Delay &rarr; Reverb &rarr; Filter</option>
          <option value="4">Reverb &rarr; Filter &rarr; Delay</option>
          <option value="5">Reverb &rarr; Delay &rarr; Filter</option>
        </select>
      </div>
      <div class="dsp-slider">
        <label>Reverb</label>
        <select id="reverb-type" onchange="onReverbTypeChange()" style="flex:1;font-family:var(--font);font-size:0.75rem;background:var(--bg);color:var(--text-hi);border:1px solid var(--border);padding:4px">
          <option value="0">Spring</option><option value="1">Plate</option>
          <option value="2">Hall</option><option value="3">Schroeder</option>
        </select>
      </div>
      <div class="dsp-slider">
        <label>Delay</label>
        <select id="delay-type" onchange="onDelayTypeChange()" style="flex:1;font-family:var(--font);font-size:0.75rem;background:var(--bg);color:var(--text-hi);border:1px solid var(--border);padding:4px">
          <option value="0">Tape</option><option value="1">Digital</option>
        </select>
      </div>
    </div>
    <div class="dsp-group" id="tape-params">
      <h4>Tape Delay</h4>
      <div class="dsp-slider">
        <label>Wobble</label>
        <input type="range" id="wobble" min="0" max="100" value="100" oninput="updateRange('wobble')">
        <span class="dsp-val" id="wobble-val">100%</span>
      </div>
      <div class="dsp-slider">
        <label>Flutter</label>
        <input type="range" id="flutter" min="0" max="100" value="100" oninput="updateRange('flutter')">
        <span class="dsp-val" id="flutter-val">100%</span>
      </div>
    </div>
    <div class="dsp-group">
      <h4>Tape Saturator</h4>
      <div class="dsp-slider">
        <label>Mix</label>
        <input type="range" id="saturator-mix" min="0" max="100" value="0" oninput="updateRange('saturator-mix')">
        <span class="dsp-val" id="saturator-mix-val">0%</span>
      </div>
      <div class="dsp-slider">
        <label>Drive</label>
        <input type="range" id="saturator-drive" min="0" max="100" value="50" oninput="updateRange('saturator-drive')">
        <span class="dsp-val" id="saturator-drive-val">50%</span>
      </div>
    </div>
    <div class="dsp-group">
      <h4>Modulation FX</h4>
      <div class="dsp-slider">
        <label>Phaser</label>
        <input type="range" id="phaser-mix" min="0" max="100" value="0" oninput="updateRange('phaser-mix')">
        <span class="dsp-val" id="phaser-mix-val">0%</span>
      </div>
      <div class="dsp-slider">
        <label>Chorus</label>
        <input type="range" id="chorus-mix" min="0" max="100" value="0" oninput="updateRange('chorus-mix')">
        <span class="dsp-val" id="chorus-mix-val">0%</span>
      </div>
      <div class="dsp-slider">
        <label>Flanger</label>
        <input type="range" id="flanger-mix" min="0" max="100" value="0" oninput="updateRange('flanger-mix')">
        <span class="dsp-val" id="flanger-mix-val">0%</span>
      </div>
    </div>
  </div>
  <div class="card">
    <h3>Behavior</h3>
    <div class="toggle-row"><span>Linked Pitch / LFO</span><label class="toggle"><input type="checkbox" id="lfo-link" checked><span class="slider"></span></label></div>
    <div class="toggle-row"><span>Super Drip Reverb</span><label class="toggle"><input type="checkbox" id="super-drip" checked><span class="slider"></span></label></div>
    <div class="dsp-slider" style="padding:10px 0;border-bottom:1px solid var(--border)">
      <label style="min-width:auto;flex:1">Filter Sweep</label>
      <select id="sweep-dir" style="width:auto;padding:6px;font-family:var(--font);font-size:0.75rem;background:var(--bg);color:var(--text-hi);border:1px solid var(--border)">
        <option value="-1">Down (Dub)</option><option value="0">Flat</option><option value="1">Up (Bright)</option>
      </select>
    </div>
  </div>
  <div class="card">
    <h3>LED</h3>
    <div class="dsp-slider">
      <label>Brightness</label>
      <input type="range" id="led-brightness" min="5" max="100" value="100" oninput="updateRange('led-brightness')">
      <span class="dsp-val" id="led-brightness-val">100%</span>
    </div>
    <div class="toggle-row"><span>Night Mode</span><label class="toggle"><input type="checkbox" id="led-night-mode"><span class="slider"></span></label></div>
    <p style="font-size:0.6rem;color:var(--text-lo);margin-top:6px">Night mode caps brightness at 15% for dark rooms.</p>
  </div>
  <button class="btn btn-primary btn-block" onclick="applyOptions()" style="margin-top:8px">Apply Options</button>
</div>

<!-- WIFI TAB -->
<div id="wifi" class="panel">
  <div class="card">
    <h3>Networks</h3>
    <button class="btn btn-secondary" onclick="scanWifi()" style="margin-bottom:12px">Scan</button>
    <div id="wifi-networks" class="network-list"></div>
  </div>
  <div class="card">
    <h3>Connect</h3>
    <div class="form-group"><label>SSID</label><input type="text" id="wifi-ssid"></div>
    <div class="form-group"><label>Password</label><div style="position:relative"><input type="password" id="wifi-pass" style="padding-right:36px"><button type="button" onclick="const i=document.getElementById('wifi-pass');const s=i.type==='password';i.type=s?'text':'password';this.textContent=s?'\u25CF':'\u25CB'" style="position:absolute;right:6px;top:50%;transform:translateY(-50%);background:none;border:none;color:var(--text);font-size:1rem;cursor:pointer;padding:2px 4px" title="Show/hide password">&#9675;</button></div></div>
    <div style="display:flex;gap:8px">
      <button class="btn btn-secondary" onclick="connectWifi()" style="flex:1">Save Only</button>
      <button class="btn btn-primary" id="btn-wifi-test" onclick="testWifi()" style="flex:1">Test &amp; Save</button>
    </div>
    <div id="wifi-test-result" style="display:none;margin-top:12px;padding:10px;font-size:0.75rem;border:1px solid var(--border)"></div>
  </div>
  <div class="card">
    <h3>Status</h3>
    <div id="wifi-status" style="font-size:0.8rem;color:var(--text-lo)">Not connected</div>
  </div>
</div>

<!-- SYSTEM TAB -->
<div id="system" class="panel">
  <div class="card">
    <h3>Device Status</h3>
    <div id="sys-stats">
      <div class="sys-stat"><span class="sys-label">CPU Temp</span><span class="sys-value" id="sys-cpu">--</span></div>
      <div class="sys-stat"><span class="sys-label">Uptime</span><span class="sys-value" id="sys-uptime">--</span></div>
      <div class="sys-stat"><span class="sys-label">Memory</span><span class="sys-value" id="sys-mem">--</span></div>
      <div class="sys-stat"><span class="sys-label">WiFi</span><span class="sys-value" id="sys-wifi">--</span></div>
      <div class="sys-stat"><span class="sys-label">Branch</span><span class="sys-value" id="sys-branch">--</span></div>
      <div class="sys-stat"><span class="sys-label">Commit</span><span class="sys-value" id="sys-commit">--</span></div>
    </div>
    <div style="display:flex;gap:8px;margin-top:12px">
      <button class="btn btn-secondary btn-sm" onclick="confirmAction('Restart the siren service?',restartService)" style="flex:1">Restart Siren</button>
      <button class="btn btn-danger btn-sm" onclick="confirmAction('Reboot the device? This will take ~30 seconds.',rebootDevice)" style="flex:1">Reboot Device</button>
    </div>
  </div>
  <div class="card">
    <div style="display:flex;align-items:center;cursor:pointer" onclick="var b=document.getElementById('syslog-body');var a=document.getElementById('syslog-arrow');if(b.style.display==='none'){b.style.display='block';a.textContent='\\u25BC'}else{b.style.display='none';a.textContent='\\u25B6'}">
      <h3 style="margin:0">System Log <span id="syslog-arrow" style="font-size:0.6rem">&#9654;</span></h3>
    </div>
    <div id="syslog-body" style="display:none;margin-top:10px">
      <div style="display:flex;gap:8px;margin-bottom:8px;align-items:center">
        <label style="font-size:0.7rem;color:var(--text-lo);display:flex;align-items:center;gap:4px;cursor:pointer"><input type="checkbox" id="syslog-auto" checked onchange="toggleSyslogAuto()"> Auto-refresh</label>
        <button class="btn btn-secondary btn-sm" onclick="loadSysLog()" style="font-size:0.6rem;padding:3px 8px;margin-left:auto">Refresh</button>
      </div>
      <pre id="syslog-content" style="background:var(--bg);border:1px solid var(--border);padding:10px;max-height:250px;overflow:auto;font-family:var(--font);font-size:0.6rem;line-height:1.6;color:var(--text);white-space:pre-wrap;word-break:break-all"></pre>
    </div>
  </div>
  <div class="card">
    <h3>Updates</h3>
    <p id="version-info" style="color:var(--text-lo);margin-bottom:8px;font-size:0.75rem">--</p>
    <div id="main-update-notice" style="display:none;background:var(--bg);border:1px solid var(--accent);padding:10px;margin-bottom:12px">
      <p id="main-update-text" style="color:var(--accent);font-size:0.75rem;font-weight:700;margin-bottom:8px"></p>
      <button class="btn btn-primary btn-sm" onclick="revertToMain()">Revert to main</button>
    </div>
    <div class="form-group">
      <label>Branch</label>
      <select id="update-branch" style="width:100%;padding:8px;border:1px solid var(--border);background:var(--bg);color:var(--text);font-size:0.85rem">
        <option value="main">main</option>
      </select>
      <div id="branch-warn-toggle" style="margin-top:6px">
        <span onclick="document.getElementById('branch-warn-toggle').style.display='none';document.getElementById('branch-warn-confirm').style.display='block'" style="font-size:0.6rem;color:var(--text-lo);cursor:pointer;text-decoration:underline">Show experimental branches</span>
      </div>
      <div id="branch-warn-confirm" style="display:none;margin-top:6px">
        <p style="color:var(--danger);font-size:0.7rem;margin-bottom:8px">Experimental branches are untested and may cause unexpected behavior or require a full reflash to recover.</p>
        <button class="btn btn-secondary btn-sm" onclick="document.getElementById('branch-warn-confirm').style.display='none';loadBranches()" style="color:var(--danger);border-color:var(--danger);font-size:0.55rem">I understand, show all branches</button>
      </div>
    </div>
    <button class="btn btn-secondary" onclick="checkUpdate()" id="btn-check">Check for Updates</button>
    <div id="update-result" style="margin-top:12px"></div>
    <div id="update-progress" style="display:none;margin-top:12px">
      <div style="font-size:0.8rem;color:var(--text-lo);margin-bottom:6px" id="update-stage-text">Preparing...</div>
      <div style="background:var(--card);border:1px solid var(--border);height:22px;overflow:hidden">
        <div id="update-bar" style="height:100%;width:0%;background:var(--accent);transition:width 0.4s ease"></div>
      </div>
      <div style="font-size:0.7rem;color:var(--text-lo);margin-top:4px;text-align:right" id="update-pct-text">0%</div>
    </div>
    <div id="update-log-section" style="display:none;margin-top:12px">
      <div style="display:flex;align-items:center;justify-content:space-between;cursor:pointer;padding:6px 0" onclick="toggleUpdateLog()">
        <span style="font-size:0.65rem;font-weight:700;color:var(--text-lo);letter-spacing:2px;text-transform:uppercase">Verbose Log <span id="log-arrow">&#9654;</span></span>
        <button class="btn btn-secondary btn-sm" onclick="event.stopPropagation();copyUpdateLog()" style="font-size:0.55rem;padding:4px 8px">Copy</button>
      </div>
      <pre id="update-log" style="display:none;background:var(--bg);border:1px solid var(--border);padding:10px;margin-top:4px;max-height:300px;overflow:auto;font-family:var(--font);font-size:0.65rem;line-height:1.5;color:var(--text);white-space:pre-wrap;word-break:break-all"></pre>
    </div>
    <div id="update-reboot" style="display:none;margin-top:12px">
      <button class="btn btn-primary" onclick="confirmAction('Restart the service to apply the update?',restartService)" style="width:100%">Restart to Apply</button>
    </div>
  </div>
  <div class="card">
    <h3>Theme</h3>
    <div style="font-size:0.6rem;font-weight:700;letter-spacing:2px;text-transform:uppercase;color:var(--text-lo);margin-bottom:6px">Dark</div>
    <div class="theme-grid" id="theme-grid-dark"></div>
    <div style="font-size:0.6rem;font-weight:700;letter-spacing:2px;text-transform:uppercase;color:var(--text-lo);margin:12px 0 6px">Light</div>
    <div class="theme-grid" id="theme-grid-light"></div>
    <button class="btn btn-primary" id="btn-save-theme" onclick="saveTheme()" style="margin-top:12px;display:none;width:100%">Save Theme</button>
  </div>
  <div class="card">
    <h3>Backup &amp; Restore</h3>
    <button class="btn btn-secondary" onclick="downloadBackup()" style="margin-bottom:8px">Download Backup</button>
    <div class="form-group">
      <label>Restore from file</label>
      <input type="file" id="restore-file" accept=".json" style="font-size:0.75rem">
    </div>
    <button class="btn btn-secondary" onclick="confirmAction('Restore will overwrite all presets. Continue?',restoreBackup)">Restore</button>
  </div>
  <div class="card">
    <h3>Shell</h3>
    <div id="shell-warn1">
      <p style="color:var(--danger);font-size:0.75rem;margin-bottom:10px">Running shell commands can permanently damage your device.</p>
      <button class="btn btn-secondary btn-sm" onclick="document.getElementById('shell-warn1').style.display='none';document.getElementById('shell-warn2').style.display='block'" style="color:var(--danger);border-color:var(--danger)">I understand, continue</button>
    </div>
    <div id="shell-warn2" style="display:none">
      <p style="color:var(--danger);font-size:0.75rem;font-weight:700;margin-bottom:10px">WARNING: Incorrect commands can brick this device and require a full reflash. Are you sure you want to proceed?</p>
      <button class="btn btn-secondary btn-sm" onclick="document.getElementById('shell-warn2').style.display='none';document.getElementById('shell-actual').style.display='block'" style="color:var(--danger);border-color:var(--danger)">Yes, open shell</button>
    </div>
    <div id="shell-actual" style="display:none">
      <pre id="shell-output" style="background:#111;border:1px solid var(--border);padding:10px;max-height:300px;min-height:80px;overflow:auto;font-family:monospace;font-size:0.65rem;line-height:1.5;color:#0f0;white-space:pre-wrap;word-break:break-all"></pre>
      <div style="display:flex;gap:6px;margin-top:8px">
        <input type="text" id="shell-cmd" placeholder="Enter command..." style="flex:1;padding:8px;font-family:monospace;font-size:0.8rem;background:var(--bg);border:1px solid var(--border);color:var(--text)" onkeydown="if(event.key==='Enter')runShell()" autocomplete="off" autocorrect="off" spellcheck="false">
        <button class="btn btn-primary btn-sm" onclick="runShell()" id="btn-shell-run">Run</button>
      </div>
    </div>
  </div>
  <div class="card">
    <button class="btn btn-danger btn-block" onclick="exitAP()">Return to Siren Mode</button>
  </div>
</div>
</div>

<!-- Confirm Dialog -->
<div class="confirm-overlay" id="confirm-overlay" style="display:none;position:fixed;inset:0;background:rgba(0,0,0,0.7);z-index:998;align-items:center;justify-content:center">
  <div style="background:var(--surface);border:2px solid var(--accent);padding:24px;max-width:320px;text-align:center">
    <p id="confirm-msg" style="font-size:0.8rem;color:var(--text-hi);margin-bottom:16px"></p>
    <div style="display:flex;gap:8px;justify-content:center">
      <button class="btn btn-secondary btn-sm" onclick="closeConfirm()">Cancel</button>
      <button class="btn btn-danger btn-sm" id="confirm-ok" onclick="doConfirm()">Confirm</button>
    </div>
  </div>
</div>

<div id="toast" class="toast"></div>

<script>
// State
let allPresets = {};
let libraryByCategory = {};
let activeBank = 0, activePreset = 0;
let targetBank = null, targetSlot = null;
const BANK_NAMES = ['user', 'standard', 'experimental'];
let isOnline = true;
let heartbeatTimer = null;
let reconnectDelay = 3000;
let previewingIndex = -1;
let confirmCallback = null;
let dspThrottle = {};
let encoderParams = [];
let livePollTimer = null;
let livePollBusy = false;
const activeDragging = new Set();

// Connection resilience
function startHeartbeat() {
  if (heartbeatTimer) clearInterval(heartbeatTimer);
  heartbeatTimer = setInterval(async () => {
    if (livePollTimer) return;  // live poll handles connection health
    try {
      const r = await fetch('/api/dsp/state', {signal: AbortSignal.timeout(5000)});
      if (r.ok) { setOnline(true); reconnectDelay = 3000; }
      else setOnline(false);
    } catch(e) { setOnline(false); }
  }, 3000);
}
function setOnline(online) {
  if (isOnline === online) return;
  isOnline = online;
  document.getElementById('conn-dot').className = 'conn-dot' + (online ? '' : ' offline');
  document.getElementById('offline-banner').className = 'offline-banner' + (online ? '' : ' show');
  if (online) { loadPresets(); loadDspState(); toast('Reconnected'); }
}

// Confirm dialog
function confirmAction(msg, cb) {
  document.getElementById('confirm-msg').textContent = msg;
  document.getElementById('confirm-overlay').style.display = 'flex';
  confirmCallback = cb;
}
function closeConfirm() { document.getElementById('confirm-overlay').style.display = 'none'; confirmCallback = null; }
function doConfirm() { const cb = confirmCallback; closeConfirm(); if (cb) cb(); }

// Debounced button helper
function debounceBtn(btn, ms) {
  if (!ms) ms = 500;
  btn.disabled = true;
  setTimeout(() => btn.disabled = false, ms);
}

// Tab Navigation
function showTab(id) {
  document.querySelectorAll('.panel').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
  document.getElementById(id).classList.add('active');
  const tabs = ['live','presets','encoders','options','wifi','system'];
  const tabEls = document.querySelectorAll('.tab');
  tabs.forEach((t, i) => { if (t === id && tabEls[i]) tabEls[i].classList.add('active'); });
  if (id !== 'system' && typeof stopSyslogPoll === 'function') stopSyslogPoll();
  if (id === 'live') { loadDspState(); startLivePoll(); } else { stopLivePoll(); }
  if (id === 'presets') { loadPresets(); startPresetPoll(); } else { stopPresetPoll(); }
  if (id === 'wifi') { loadWifiStatus(); checkWifiTestResult(); }
  if (id === 'system') { loadSystemInfo(); loadSysLog(); startSyslogPoll(); }
  if (id === 'encoders') { loadEncoderMap(); loadSensitivity(); }
  if (id === 'options') { }
}

// Toast
function toast(msg, err, duration) {
  const t = document.getElementById('toast');
  t.textContent = msg;
  t.style.background = err ? 'var(--danger)' : 'var(--accent)';
  t.classList.add('show');
  setTimeout(() => t.classList.remove('show'), duration||2500);
}

function esc(s) { const d = document.createElement('div'); d.textContent = s; return d.innerHTML; }

function syncPresetHighlight(newBank, newPreset) {
  if (newBank !== undefined && newPreset !== undefined &&
      (newBank !== activeBank || newPreset !== activePreset)) {
    document.querySelectorAll('.slot.active-slot').forEach(function(el){el.classList.remove('active-slot')});
    document.querySelectorAll('.led.active').forEach(function(el){el.classList.remove('active')});
    document.querySelectorAll('.slot-btn.active').forEach(function(el){el.classList.remove('active')});
    activeBank = newBank; activePreset = newPreset;
    var newBankName = BANK_NAMES[activeBank];
    var sl = document.getElementById('slot-'+newBankName+'-'+activePreset);
    if (sl) {
      sl.classList.add('active-slot');
      var led = sl.querySelector('.led'); if (led) led.classList.add('active');
      var btn = sl.querySelector('.slot-btn'); if (btn) btn.classList.add('active');
    }
    document.querySelectorAll('.slot.target-slot').forEach(function(el){el.classList.remove('target-slot')});
    targetBank = newBankName; targetSlot = activePreset;
    if (sl) sl.classList.add('target-slot');
  }
}

// Live DSP Controls
function applyDspState(d, force) {
  if (force || !activeDragging.has('freq')) {
    document.getElementById('dsp-freq').value = d.freq || 440;
    document.getElementById('dsp-freq-val').textContent = Math.round(d.freq) + ' Hz';
  }
  if (force || !activeDragging.has('lfo_rate')) {
    setLogSlider('lfo_rate', d.lfo_rate, 0.1, 20);
    document.getElementById('dsp-lfo_rate-val').textContent = (d.lfo_rate||0).toFixed(2) + ' Hz';
  }
  if (force || !activeDragging.has('lfo_depth')) {
    document.getElementById('dsp-lfo_depth').value = Math.round((d.lfo_depth||0)*100);
    document.getElementById('dsp-lfo_depth-val').textContent = Math.round((d.lfo_depth||0)*100)+'%';
  }
  if (force || !activeDragging.has('filter_cutoff')) {
    setLogSlider('filter_cutoff', d.filter_cutoff, 20, 20000);
    document.getElementById('dsp-filter_cutoff-val').textContent = Math.round(d.filter_cutoff)+' Hz';
  }
  if (force || !activeDragging.has('filter_reso')) {
    document.getElementById('dsp-filter_reso').value = Math.round((d.filter_reso||0)*100);
    document.getElementById('dsp-filter_reso-val').textContent = Math.round((d.filter_reso||0)*100)+'%';
  }
  if (force || !activeDragging.has('delay_time')) {
    setLogSlider('delay_time', d.delay_time, 0.001, 1.0);
    document.getElementById('dsp-delay_time-val').textContent = Math.round((d.delay_time||0)*1000)+' ms';
  }
  if (force || !activeDragging.has('delay_feedback')) {
    document.getElementById('dsp-delay_feedback').value = Math.round((d.delay_feedback||0)*100);
    document.getElementById('dsp-delay_feedback-val').textContent = Math.round((d.delay_feedback||0)*100)+'%';
  }
  if (force || !activeDragging.has('delay_mix')) {
    document.getElementById('dsp-delay_mix').value = Math.round((d.delay_mix||0)*100);
    document.getElementById('dsp-delay_mix-val').textContent = Math.round((d.delay_mix||0)*100)+'%';
  }
  if (force || !activeDragging.has('release_time')) {
    setLogSlider('release_time', d.release_time, 0.01, 5.0);
    document.getElementById('dsp-release_time-val').textContent = Math.round((d.release_time||0)*1000)+' ms';
  }
  if (force || !activeDragging.has('reverb_mix')) {
    document.getElementById('dsp-reverb_mix').value = Math.round((d.reverb_mix||0)*100);
    document.getElementById('dsp-reverb_mix-val').textContent = Math.round((d.reverb_mix||0)*100)+'%';
  }
  if (force || !activeDragging.has('waveform')) {
    document.getElementById('dsp-waveform').value = d.waveform || 0;
  }
  if (force || !activeDragging.has('lfo_waveform')) {
    document.getElementById('dsp-lfo-waveform').value = d.lfo_waveform || 0;
  }
  if (force || !activeDragging.has('pitch_env')) {
    updatePitchEnvUI(d.pitch_env || 0);
  }
  // Sync preset highlight with hardware button presses
  syncPresetHighlight(d.active_bank, d.active_preset);
}

async function loadDspState() {
  try {
    const r = await fetch('/api/dsp/state');
    applyDspState(await r.json(), true);
  } catch(e) {}
}

async function pollDspState() {
  if (livePollBusy) return;
  livePollBusy = true;
  try {
    const r = await fetch('/api/dsp/state', {signal: AbortSignal.timeout(2000)});
    if (!r.ok) { setOnline(false); return; }
    if (!isOnline) setOnline(true);
    applyDspState(await r.json(), false);
  } catch(e) { setOnline(false); }
  finally { livePollBusy = false; }
}
function startLivePoll() { if (!livePollTimer) livePollTimer = setInterval(pollDspState, 200); }
function stopLivePoll() { if (livePollTimer) { clearInterval(livePollTimer); livePollTimer = null; } }

let presetPollTimer = null;
function startPresetPoll() { if (!presetPollTimer) presetPollTimer = setInterval(pollPresetState, 500); }
function stopPresetPoll() { if (presetPollTimer) { clearInterval(presetPollTimer); presetPollTimer = null; } }
async function pollPresetState() {
  try {
    const r = await fetch('/api/dsp/state', {signal: AbortSignal.timeout(2000)});
    if (!r.ok) return;
    const d = await r.json();
    syncPresetHighlight(d.active_bank, d.active_preset);
  } catch(e) {}
}

function setLogSlider(name, value, min, max) {
  // Map log value to 0-1000 slider position
  const logMin = Math.log(min), logMax = Math.log(max);
  const pos = Math.round((Math.log(value) - logMin) / (logMax - logMin) * 1000);
  const el = document.getElementById('dsp-' + name);
  if (el) el.value = Math.max(0, Math.min(1000, pos));
}

function logSliderToValue(sliderVal, min, max) {
  const logMin = Math.log(min), logMax = Math.log(max);
  return Math.exp(logMin + (sliderVal / 1000) * (logMax - logMin));
}

function setDspParam(name, value) {
  fetch('/api/dsp/param', { method: 'POST',
    headers: {'Content-Type':'application/x-www-form-urlencoded'},
    body: 'name='+name+'&value='+value }).catch(e => {});
}

function throttledSetParam(name, value) {
  if (dspThrottle[name]) cancelAnimationFrame(dspThrottle[name]);
  dspThrottle[name] = requestAnimationFrame(() => { setDspParam(name, value); });
}

function onDspSlider(name, val) {
  val = parseFloat(val);
  document.getElementById('dsp-'+name+'-val').textContent = Math.round(val)+' Hz';
  throttledSetParam(name, val);
}

function onDspSliderLog(name, val, min, max) {
  const v = logSliderToValue(parseFloat(val), min, max);
  const el = document.getElementById('dsp-'+name+'-val');
  if (v >= 1) el.textContent = Math.round(v) + (max > 100 ? ' Hz' : ' ms');
  else if (v >= 0.01) el.textContent = (v*1000).toFixed(0) + ' ms';
  else el.textContent = v.toFixed(3);
  if (name === 'lfo_rate') el.textContent = v.toFixed(2) + ' Hz';
  if (name === 'release_time') el.textContent = Math.round(v*1000) + ' ms';
  if (name === 'delay_time') el.textContent = Math.round(v*1000) + ' ms';
  throttledSetParam(name, v);
}

function onDspSliderPct(name, val, maxVal) {
  const mx = maxVal || 1.0;
  const v = (parseFloat(val)/100) * mx;
  document.getElementById('dsp-'+name+'-val').textContent = Math.round(parseFloat(val))+'%';
  throttledSetParam(name, v);
}

function updatePitchEnvUI(val) {
  const btns = document.querySelectorAll('#live .seg-btn');
  btns.forEach((b,i) => b.classList.toggle('active', (i-1) === val));
}

async function resetDefaults() {
  try {
    const r = await fetch('/api/dsp/reset', {method:'POST'});
    if (r.ok) { toast('Reset to defaults'); loadDspState(); loadOptions(); }
    else toast('Reset failed', true);
  } catch(e) { toast('Error: '+e, true); }
}

async function doLiveSave() {
  const bank = document.getElementById('live-save-bank').value;
  const slot = document.getElementById('live-save-slot').value;
  const name = document.getElementById('live-save-name').value || 'Preset';
  try {
    const r = await fetch('/api/presets/bank/save', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'bank='+bank+'&slot='+slot+'&name='+encodeURIComponent(name)});
    if (r.ok) { toast('Saved to '+bank+' slot '+(parseInt(slot)+1)); loadPresets(); }
    else toast('Save failed', true);
  } catch(e) { toast('Error: '+e, true); }
}

// Options
function onReverbTypeChange() {}
function onDelayTypeChange() {
  document.getElementById('tape-params').style.display =
    parseInt(document.getElementById('delay-type').value) === 0 ? 'block' : 'none';
}
function updateRange(id) {
  document.getElementById(id+'-val').textContent = document.getElementById(id).value+'%';
}

// Presets
function selectTargetSlot(bank, slot) {
  if (targetBank === bank && targetSlot === slot) {
    document.querySelectorAll('.slot.target-slot').forEach(el => el.classList.remove('target-slot'));
    targetBank = BANK_NAMES[activeBank]; targetSlot = activePreset;
    const ae = document.getElementById('slot-'+targetBank+'-'+targetSlot);
    if (ae) ae.classList.add('target-slot');
    document.getElementById('library-browser').style.display = 'none';
    document.getElementById('target-hint').textContent = 'Tap a slot to select it, then pick a preset below';
    return;
  }
  targetBank = bank; targetSlot = slot;
  document.querySelectorAll('.slot.target-slot').forEach(el => el.classList.remove('target-slot'));
  const el = document.getElementById('slot-'+bank+'-'+slot);
  if (el) el.classList.add('target-slot');
  document.getElementById('target-hint').textContent = 'Target: '+bank.toUpperCase()+' slot '+(slot+1)+' \u2014 pick a preset below';
  document.getElementById('target-label').textContent = '\u2192 '+bank.toUpperCase()+' SLOT '+(slot+1);
  document.getElementById('library-browser').style.display = 'block';
  renderLibrary();
}

function renderLibrary() {
  const list = document.getElementById('library-list');
  let html = '';
  Object.keys(libraryByCategory).forEach(cat => {
    html += '<div class="lib-category">'+esc(cat)+'</div>';
    libraryByCategory[cat].forEach(p => {
      const isPreviewing = previewingIndex === p.index;
      html += '<div class="lib-preset">'+
        '<span class="lib-name">'+esc(p.name)+'</span>'+
        '<div class="lib-actions">'+
          '<div class="lib-btn'+(isPreviewing?' previewing':'')+'" onclick="event.stopPropagation();doPreviewPreset('+p.index+')">'+(isPreviewing?'Stop':'Preview')+'</div>'+
          '<div class="lib-btn primary" onclick="event.stopPropagation();doLoadPreset('+p.index+',\''+esc(p.name)+'\')">Load</div>'+
        '</div></div>';
    });
  });
  list.innerHTML = html;
}

function renderBankSlots(bankName, presets, gridId) {
  const grid = document.getElementById(gridId);
  const bankIdx = BANK_NAMES.indexOf(bankName);
  grid.innerHTML = presets.map((p,i) => {
    const isActive = activeBank === bankIdx && activePreset === i;
    const isTarget = targetBank === bankName && targetSlot === i;
    return '<div class="slot'+(isActive?' active-slot':'')+(isTarget?' target-slot':'')+'" id="slot-'+bankName+'-'+i+'" onclick="selectTargetSlot(\''+bankName+'\','+i+')">'+
      '<div class="slot-num"><div class="led'+(isActive?' active':'')+'"></div><span>0'+(i+1)+'</span></div>'+
      '<div class="slot-name">'+esc(p.name||'Empty')+'</div>'+
      '<div class="slot-cat">'+(p.saved?'Saved':'Factory')+'</div>'+
      '<div class="slot-actions">'+
        '<div class="slot-btn'+(isActive?' active':'')+'" onclick="event.stopPropagation();applyBankPreset(\''+bankName+'\','+i+')">Apply</div>'+
        '<div class="slot-btn" onclick="event.stopPropagation();showSaveInput(\''+bankName+'\','+i+')">Save</div>'+
      '</div>'+
      '<div class="save-inline" id="save-inline-'+bankName+'-'+i+'">'+
        '<input type="text" placeholder="Name" id="save-name-'+bankName+'-'+i+'" value="'+esc(p.name||'')+'" onclick="event.stopPropagation()" onkeydown="if(event.key===\'Enter\')doSave(\''+bankName+'\','+i+')">'+
        '<button onclick="event.stopPropagation();doSave(\''+bankName+'\','+i+')">OK</button>'+
      '</div></div>';
  }).join('');
}

async function applyBankPreset(bank, index) {
  try {
    const r = await fetch('/api/presets/apply', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'category='+bank+'&index='+index});
    if (r.ok) { toast('Applied'); loadPresets(); loadDspState(); } else toast('Apply failed',true);
  } catch(e) { toast('Error: '+e,true); }
}

function showSaveInput(bank, i) {
  document.querySelectorAll('.save-inline.show').forEach(el => el.classList.remove('show'));
  const el = document.getElementById('save-inline-'+bank+'-'+i);
  if (el) { el.classList.add('show'); el.querySelector('input').focus(); }
}

async function doSave(bank, slot) {
  const input = document.getElementById('save-name-'+bank+'-'+slot);
  const name = input ? input.value || 'Preset' : 'Preset';
  try {
    const r = await fetch('/api/presets/bank/save', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'bank='+bank+'&slot='+slot+'&name='+encodeURIComponent(name)});
    if (r.ok) { toast('Saved to '+bank+' slot '+(slot+1)); loadPresets(); } else toast('Save failed',true);
  } catch(e) { toast('Error: '+e,true); }
}

function buildCategoryDropdown(data) {
  libraryByCategory = {};
  (data.library||[]).forEach(p => {
    const cat = p.category||'Other';
    if (!libraryByCategory[cat]) libraryByCategory[cat] = [];
    libraryByCategory[cat].push(p);
  });
  if (targetBank !== null) renderLibrary();
}

async function doLoadPreset(index, name) {
  if (targetBank === null || targetSlot === null) { toast('Select a target slot first',true); return; }
  try {
    const r = await fetch('/api/presets/bank/load', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'bank='+targetBank+'&slot='+targetSlot+'&category=library&index='+index});
    if (r.ok) { toast('Loaded "'+name+'" \u2192 '+targetBank.toUpperCase()+' slot '+(targetSlot+1)); loadPresets(); }
    else toast('Load failed',true);
  } catch(e) { toast('Error: '+e,true); }
}

async function doPreviewPreset(index) {
  try {
    if (previewingIndex === index) {
      // Stop preview
      await fetch('/api/preview/stop', {method:'POST'});
      previewingIndex = -1;
      toast('Preview stopped');
      renderLibrary();
      return;
    }
    // Stop any existing preview first
    if (previewingIndex >= 0) await fetch('/api/preview/stop', {method:'POST'});
    const r = await fetch('/api/preview/start', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'category=library&index='+index});
    if (r.ok) { previewingIndex = index; toast('Previewing'); renderLibrary(); }
    else toast('Preview failed',true);
  } catch(e) { toast('Error: '+e,true); }
}

async function loadPresets() {
  try {
    const r = await fetch('/api/presets');
    const d = await r.json();
    allPresets = d;
    activeBank = d.active_bank||0;
    activePreset = d.active_preset||0;
    renderBankSlots('user', d.user||[], 'slot-grid-user');
    renderBankSlots('standard', d.standard||[], 'slot-grid-standard');
    renderBankSlots('experimental', d.experimental||[], 'slot-grid-experimental');
    if (targetBank === null) {
      targetBank = BANK_NAMES[activeBank]; targetSlot = activePreset;
      const ae = document.getElementById('slot-'+targetBank+'-'+targetSlot);
      if (ae) ae.classList.add('target-slot');
    }
    buildCategoryDropdown(d);
  } catch(e) { console.error(e); }
}

async function loadOptions() {
  try {
    const r = await fetch('/api/siren/options');
    const d = await r.json();
    document.getElementById('reverb-type').value = d.reverb_type||0;
    document.getElementById('delay-type').value = d.delay_type||0;
    onDelayTypeChange();
    document.getElementById('wobble').value = Math.round((d.tape_wobble||1)*100);
    document.getElementById('flutter').value = Math.round((d.tape_flutter||1)*100);
    updateRange('wobble'); updateRange('flutter');
    document.getElementById('fx-chain').value = d.fx_chain||0;
    document.getElementById('lfo-link').checked = d.lfo_pitch_link !== false;
    document.getElementById('super-drip').checked = d.super_drip !== false;
    document.getElementById('sweep-dir').value = d.sweep_dir||-1;
    document.getElementById('saturator-mix').value = Math.round((d.saturator_mix||0)*100);
    document.getElementById('saturator-drive').value = Math.round((d.saturator_drive!=null?d.saturator_drive:0.5)*100);
    updateRange('saturator-mix'); updateRange('saturator-drive');
    document.getElementById('phaser-mix').value = Math.round((d.phaser_mix||0)*100);
    document.getElementById('chorus-mix').value = Math.round((d.chorus_mix||0)*100);
    document.getElementById('flanger-mix').value = Math.round((d.flanger_mix||0)*100);
    updateRange('phaser-mix'); updateRange('chorus-mix'); updateRange('flanger-mix');
    document.getElementById('led-brightness').value = Math.round((d.led_brightness!=null?d.led_brightness:1)*100);
    updateRange('led-brightness');
    document.getElementById('led-night-mode').checked = d.led_night_mode === true;
    // Sync theme from server
    if (d.theme) {
      savedTheme = d.theme;
      document.documentElement.setAttribute('data-theme', d.theme);
      try { localStorage.setItem('dubsiren-theme', d.theme); } catch(e) {}
      renderThemeGrid();
    }
  } catch(e) { console.error(e); }
}

async function applyOptions() {
  const body = [
    'reverb_type='+document.getElementById('reverb-type').value,
    'delay_type='+document.getElementById('delay-type').value,
    'tape_wobble='+(document.getElementById('wobble').value/100),
    'tape_flutter='+(document.getElementById('flutter').value/100),
    'fx_chain='+document.getElementById('fx-chain').value,
    'lfo_pitch_link='+(document.getElementById('lfo-link').checked?'1':'0'),
    'super_drip='+(document.getElementById('super-drip').checked?'1':'0'),
    'sweep_dir='+document.getElementById('sweep-dir').value,
    'saturator_mix='+(document.getElementById('saturator-mix').value/100),
    'saturator_drive='+(document.getElementById('saturator-drive').value/100),
    'phaser_mix='+(document.getElementById('phaser-mix').value/100),
    'chorus_mix='+(document.getElementById('chorus-mix').value/100),
    'flanger_mix='+(document.getElementById('flanger-mix').value/100),
    'led_brightness='+(document.getElementById('led-brightness').value/100),
    'led_night_mode='+(document.getElementById('led-night-mode').checked?'1':'0'),
    'theme='+(savedTheme||'midnight')
  ].join('&');
  try {
    const r = await fetch('/api/siren/options', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
    if (r.ok) toast('Options applied'); else toast('Failed',true);
  } catch(e) { toast('Error: '+e,true); }
}

// Encoder mapping
function updateDeviceDiagram() {
  for (let i = 0; i < 5; i++) {
    const aEl = document.getElementById('enc-a'+i);
    const bEl = document.getElementById('enc-b'+i);
    const aLabel = document.getElementById('enc-label-a'+i);
    const bLabel = document.getElementById('enc-label-b'+i);
    if (aEl && aLabel && encoderParams[aEl.value]) aLabel.textContent = encoderParams[aEl.value].label;
    if (bEl && bLabel && encoderParams[bEl.value]) bLabel.textContent = encoderParams[bEl.value].label;
  }
}

async function loadEncoderMap() {
  try {
    const r = await fetch('/api/encoders/map');
    const d = await r.json();
    encoderParams = d.params || [];
    const ba = d.bank_a || [0,1,2,3,4];
    const bb = d.bank_b || [5,6,7,8,9];
    let html = '<tr><th>Encoder</th><th>Bank A</th><th>Bank B</th></tr>';
    for (let i = 0; i < 5; i++) {
      const opts = encoderParams.map((p,pi) =>
        '<option value="'+pi+'">'+esc(p.label)+'</option>').join('');
      html += '<tr><td style="color:var(--text-hi);font-weight:700">Enc '+(i+1)+'</td>'+
        '<td><select id="enc-a'+i+'" onchange="updateDeviceDiagram()">'+opts+'</select></td>'+
        '<td><select id="enc-b'+i+'" onchange="updateDeviceDiagram()">'+opts+'</select></td></tr>';
    }
    document.getElementById('enc-table').innerHTML = html;
    for (let i = 0; i < 5; i++) {
      document.getElementById('enc-a'+i).value = ba[i];
      document.getElementById('enc-b'+i).value = bb[i];
    }
    updateDeviceDiagram();
  } catch(e) { console.error(e); }
}

async function saveEncoderMap() {
  let body = '';
  for (let i = 0; i < 5; i++) {
    if (i > 0) body += '&';
    body += 'a'+i+'='+document.getElementById('enc-a'+i).value;
    body += '&b'+i+'='+document.getElementById('enc-b'+i).value;
  }
  try {
    const r = await fetch('/api/encoders/map', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
    if (r.ok) toast('Encoder mapping saved'); else toast('Save failed',true);
  } catch(e) { toast('Error: '+e,true); }
}

function resetEncoderMap() {
  for (let i = 0; i < 5; i++) {
    document.getElementById('enc-a'+i).value = i;
    document.getElementById('enc-b'+i).value = i+5;
  }
  saveEncoderMap();
  updateDeviceDiagram();
}

// Sensitivity
let sensitivityData = [];
async function loadSensitivity() {
  try {
    const r = await fetch('/api/encoders/sensitivity');
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const d = await r.json();
    sensitivityData = d.params || [];
    let html = '';
    sensitivityData.forEach((p, i) => {
      const pct = Math.round(((p.sensitivity - 0.25) / 3.75) * 100);
      html += '<div class="dsp-slider"><label style="min-width:120px">'+esc(p.label)+'</label>'+
        '<input type="range" min="0" max="100" value="'+pct+'" id="sens-'+i+'" oninput="updateSensVal('+i+')">'+
        '<span class="dsp-val" id="sens-val-'+i+'">'+(p.sensitivity).toFixed(2)+'x</span></div>';
    });
    document.getElementById('sensitivity-list').innerHTML = html;
  } catch(e) { document.getElementById('sensitivity-list').textContent = 'Failed to load'; }
}

function updateSensVal(i) {
  const pct = parseInt(document.getElementById('sens-'+i).value);
  const val = 0.25 + (pct / 100) * 3.75;
  document.getElementById('sens-val-'+i).textContent = val.toFixed(2)+'x';
}

async function saveSensitivity() {
  let body = '';
  for (let i = 0; i < sensitivityData.length; i++) {
    const pct = parseInt(document.getElementById('sens-'+i).value);
    const val = 0.25 + (pct / 100) * 3.75;
    if (i > 0) body += '&';
    body += 's'+i+'='+val.toFixed(6);
  }
  try {
    const r = await fetch('/api/encoders/sensitivity', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
    if (r.ok) toast('Sensitivity saved'); else toast('Save failed',true);
  } catch(e) { toast('Error: '+e,true); }
}

function resetSensitivity() {
  const defaults = [1.00, 0.55, 0.51, 1.00, 0.25, 0.78, 1.00, 1.00, 0.44, 0.44, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0];
  sensitivityData.forEach((p, i) => {
    const def = defaults[i] !== undefined ? defaults[i] : 1.0;
    const pct = Math.round(((def - 0.25) / 3.75) * 100);
    document.getElementById('sens-'+i).value = pct;
    document.getElementById('sens-val-'+i).textContent = def.toFixed(2)+'x';
  });
  saveSensitivity();
}

// WiFi
async function loadWifiStatus() {
  try {
    const r = await fetch('/api/wifi/status');
    const wifi = await r.json();
    document.getElementById('wifi-status').textContent = wifi.connected ? 'Connected: '+wifi.ssid : 'Not connected';
  } catch(e) {}
}
async function scanWifi() {
  document.getElementById('wifi-networks').innerHTML = '<div class="loading"><div class="spinner"></div> Scanning...</div>';
  try {
    const r = await fetch('/api/wifi/scan');
    const d = await r.json();
    document.getElementById('wifi-networks').innerHTML =
      (d.networks||[]).map(n => '<div class="network-item" onclick="document.getElementById(\'wifi-ssid\').value=\''+esc(n.ssid)+'\'"><span>'+esc(n.ssid)+'</span><span class="signal">'+n.signal+' dBm</span></div>').join('') ||
      '<div style="color:var(--text-lo);padding:8px;font-size:0.8rem">No networks found</div>';
  } catch(e) { document.getElementById('wifi-networks').innerHTML = '<div style="color:var(--danger);padding:8px;font-size:0.8rem">Scan failed</div>'; }
}

async function connectWifi() {
  const ssid = document.getElementById('wifi-ssid').value;
  const pass = document.getElementById('wifi-pass').value;
  if (!ssid) { toast('Enter SSID',true); return; }
  try {
    const r = await fetch('/api/wifi/connect', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'ssid='+encodeURIComponent(ssid)+'&password='+encodeURIComponent(pass)});
    if (r.ok) toast('Credentials saved'); else toast('Failed',true);
  } catch(e) { toast('Error: '+e,true); }
}

let wifiTestPoll = null;
async function testWifi() {
  const ssid = document.getElementById('wifi-ssid').value;
  const pass = document.getElementById('wifi-pass').value;
  if (!ssid) { toast('Enter SSID',true); return; }
  const btn = document.getElementById('btn-wifi-test');
  btn.disabled = true;
  const res = document.getElementById('wifi-test-result');
  res.style.display = 'block';
  res.style.color = 'var(--text)';
  res.style.borderColor = 'var(--border)';
  res.innerHTML = '<div class="spinner" style="width:14px;height:14px;display:inline-block;vertical-align:middle;margin-right:6px"></div>Testing connection to \'' + esc(ssid) + '\'...';
  try {
    const r = await fetch('/api/wifi/test', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'ssid='+encodeURIComponent(ssid)+'&password='+encodeURIComponent(pass)});
    if (!r.ok) { res.textContent = 'Failed to start test'; res.style.color = 'var(--danger)'; btn.disabled = false; return; }
  } catch(e) { res.innerHTML = '<div class="spinner" style="width:14px;height:14px;display:inline-block;vertical-align:middle;margin-right:6px"></div>AP restarting — reconnect to AP network and wait...'; }
  if (wifiTestPoll) clearInterval(wifiTestPoll);
  wifiTestPoll = setInterval(async () => {
    try {
      const r = await fetch('/api/wifi/test-result', {signal: AbortSignal.timeout(3000)});
      if (!r.ok) return;
      const d = await r.json();
      if (d.state === 'done') {
        clearInterval(wifiTestPoll); wifiTestPoll = null;
        btn.disabled = false;
        res.style.borderColor = d.success ? 'var(--accent)' : 'var(--danger)';
        res.style.color = d.success ? 'var(--accent)' : 'var(--danger)';
        res.textContent = d.message;
        loadWifiStatus();
      } else if (d.state === 'running') {
        res.innerHTML = '<div class="spinner" style="width:14px;height:14px;display:inline-block;vertical-align:middle;margin-right:6px"></div>Testing connection to \'' + esc(d.ssid) + '\'...';
      }
    } catch(e) { /* AP may be restarting in non-concurrent mode */ }
  }, 2000);
}

// Check for pending test result on WiFi tab load
async function checkWifiTestResult() {
  try {
    const r = await fetch('/api/wifi/test-result', {signal: AbortSignal.timeout(3000)});
    if (!r.ok) return;
    const d = await r.json();
    if (d.state === 'done' && d.message) {
      const res = document.getElementById('wifi-test-result');
      res.style.display = 'block';
      res.style.borderColor = d.success ? 'var(--accent)' : 'var(--danger)';
      res.style.color = d.success ? 'var(--accent)' : 'var(--danger)';
      res.textContent = d.message;
    }
  } catch(e) {}
}

// System
async function loadSystemInfo() {
  try {
    const [sysR, wifiR] = await Promise.all([fetch('/api/system/info'), fetch('/api/wifi/status')]);
    const sys = await sysR.json();
    const wifi = await wifiR.json();
    document.getElementById('sys-cpu').textContent = sys.cpu_temp ? sys.cpu_temp.toFixed(1)+'\u00b0C' : 'N/A';
    let up = '';
    if (sys.uptime_days > 0) up += sys.uptime_days+'d ';
    up += sys.uptime_hours+'h '+sys.uptime_mins+'m';
    document.getElementById('sys-uptime').textContent = up;
    document.getElementById('sys-mem').textContent = sys.mem_used_mb+'MB / '+sys.mem_total_mb+'MB';
    const wifiText = wifi.connected ? 'Connected: '+wifi.ssid : 'Not connected';
    document.getElementById('sys-wifi').textContent = wifiText;
    document.getElementById('wifi-status').textContent = wifiText;
    document.getElementById('sys-branch').textContent = sys.git_branch || '--';
    document.getElementById('sys-commit').textContent = sys.git_commit || '--';
    // Update version line in Updates card
    const verEl = document.getElementById('version-info');
    if (verEl && sys.git_branch) verEl.textContent = sys.git_branch + ' @ ' + sys.git_commit;
    // Auto-check if main is ahead when on a non-main branch
    if (sys.git_branch && sys.git_branch !== 'main') checkMainUpdate();
  } catch(e) {
    document.getElementById('sys-cpu').textContent = 'Error';
  }
}

async function checkMainUpdate() {
  try {
    const r = await fetch('/api/update/check', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'branch=main'});
    const d = await r.json();
    const notice = document.getElementById('main-update-notice');
    if (d.available && d.count > 0) {
      var dateStr = '';
      if (d.latest_date) { var dt = new Date(d.latest_date); if (!isNaN(dt)) dateStr = ' (merged '+dt.toLocaleDateString()+')'; }
      document.getElementById('main-update-text').textContent = 'Device update available: '+d.count+' new commit(s) on main'+dateStr;
      notice.style.display = 'block';
    } else {
      notice.style.display = 'none';
    }
  } catch(e) {}
}

function revertToMain() {
  confirmAction('Revert to main branch? This will replace the current firmware with the latest stable release.', function() {
    document.getElementById('update-branch').value = 'main';
    installUpdate();
  });
}

// System Log
let syslogTimer = null;
async function loadSysLog() {
  try {
    const r = await fetch('/api/system/log');
    const entries = await r.json();
    const el = document.getElementById('syslog-content');
    const wasAtBottom = el.scrollTop + el.clientHeight >= el.scrollHeight - 10;
    el.textContent = entries.map(e => '[' + e.t + '] ' + e.m).join('\n');
    if (wasAtBottom) el.scrollTop = el.scrollHeight;
  } catch(e) {}
}
function startSyslogPoll() {
  stopSyslogPoll();
  if (document.getElementById('syslog-auto') && document.getElementById('syslog-auto').checked) {
    syslogTimer = setInterval(loadSysLog, 3000);
  }
}
function stopSyslogPoll() { if (syslogTimer) { clearInterval(syslogTimer); syslogTimer = null; } }
function toggleSyslogAuto() {
  if (document.getElementById('syslog-auto').checked) startSyslogPoll();
  else stopSyslogPoll();
}

// Shell
let shellHistory = [];
let shellHistIdx = -1;
async function runShell() {
  const input = document.getElementById('shell-cmd');
  const cmd = input.value.trim();
  if (!cmd) return;
  shellHistory.push(cmd);
  shellHistIdx = shellHistory.length;
  const out = document.getElementById('shell-output');
  out.textContent += (out.textContent ? '\n' : '') + '$ ' + cmd + '\n';
  input.value = '';
  document.getElementById('btn-shell-run').disabled = true;
  try {
    const r = await fetch('/api/system/shell', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({cmd:cmd})});
    const d = await r.json();
    if (d.output) out.textContent += d.output;
    if (d.exit !== 0) out.textContent += '\n[exit ' + d.exit + ']';
  } catch(e) { out.textContent += '\n[error: ' + e.message + ']'; }
  out.scrollTop = out.scrollHeight;
  document.getElementById('btn-shell-run').disabled = false;
  input.focus();
}
document.addEventListener('keydown', function(e) {
  const input = document.getElementById('shell-cmd');
  if (document.activeElement !== input) return;
  if (e.key === 'ArrowUp' && shellHistory.length) { e.preventDefault(); shellHistIdx = Math.max(0, shellHistIdx-1); input.value = shellHistory[shellHistIdx] || ''; }
  if (e.key === 'ArrowDown' && shellHistory.length) { e.preventDefault(); shellHistIdx = Math.min(shellHistory.length, shellHistIdx+1); input.value = shellHistory[shellHistIdx] || ''; }
});

async function restartService() {
  try { await fetch('/api/system/restart',{method:'POST'}); toast('Restarting...'); } catch(e) { toast('Error',true); }
}
async function rebootDevice() {
  try {
    const r = await fetch('/api/system/reboot',{method:'POST'});
    if (!r.ok) { toast('Reboot failed',true); return; }
  } catch(e) { /* server may already be going down — that's ok */ }
  showReconnecting();
}

async function restartService() {
  try {
    const r = await fetch('/api/system/restart',{method:'POST'});
    if (!r.ok) { toast('Restart failed',true); return; }
  } catch(e) { /* server may already be going down */ }
  showReconnecting();
}

function showReconnecting() {
  document.getElementById('toast').classList.remove('show');
  const overlay = document.createElement('div');
  overlay.id = 'reconnect-overlay';
  overlay.style.cssText = 'position:fixed;inset:0;background:rgba(0,0,0,0.85);z-index:999;display:flex;align-items:center;justify-content:center;flex-direction:column';
  overlay.innerHTML = '<div class="spinner" style="width:32px;height:32px;margin-bottom:16px"></div>'
    +'<div style="color:#fff;font-size:0.9rem;font-weight:700">Restarting...</div>'
    +'<div style="color:#888;font-size:0.7rem;margin-top:8px" id="reconnect-status">Waiting for device</div>';
  document.body.appendChild(overlay);
  let attempts = 0;
  const poll = setInterval(async () => {
    attempts++;
    const el = document.getElementById('reconnect-status');
    if (el) el.textContent = 'Reconnecting... ('+attempts+')';
    try {
      const r = await fetch('/api/system/info',{signal:AbortSignal.timeout(3000)});
      if (r.ok) { clearInterval(poll); location.reload(); }
    } catch(e) {}
  }, 3000);
}

const STAGE_LABELS = {idle:'Ready',backup:'Backing up...',fetch:'Fetching updates...',pull:'Pulling code...',build:'Building firmware...',deploy:'Deploying...',done:'Complete!',error:'Error'};
let updatePollTimer = null;

async function loadBranches() {
  try {
    const r = await fetch('/api/update/branches');
    const d = await r.json();
    const sel = document.getElementById('update-branch');
    const cur = sel.value;
    sel.innerHTML = (d.branches||['main']).map(b => '<option value="'+esc(b)+'"'+(b===cur?' selected':'')+'>'+esc(b)+'</option>').join('');
  } catch(e) {}
}

async function checkUpdate() {
  const branch = document.getElementById('update-branch').value;
  document.getElementById('update-result').innerHTML = '<div class="loading"><div class="spinner"></div> Checking...</div>';
  try {
    const r = await fetch('/api/update/check', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'branch='+encodeURIComponent(branch)});
    const d = await r.json();
    if (d.available) {
      document.getElementById('update-result').innerHTML = '<p style="color:var(--accent);font-size:0.8rem">'+d.count+' update(s) available on '+esc(branch)+'</p><button class="btn btn-primary" onclick="installUpdate()" style="margin-top:8px">Install</button>';
    } else {
      document.getElementById('update-result').innerHTML = '<p style="color:var(--text-lo);font-size:0.8rem">Up to date</p>';
    }
  } catch(e) { document.getElementById('update-result').innerHTML = '<p style="color:var(--danger);font-size:0.8rem">Check failed</p>'; }
}

async function installUpdate() {
  const branch = document.getElementById('update-branch').value;
  document.getElementById('update-result').innerHTML = '';
  document.getElementById('update-progress').style.display = 'block';
  document.getElementById('update-bar').style.width = '0%';
  document.getElementById('update-stage-text').textContent = 'Starting...';
  document.getElementById('update-pct-text').textContent = '0%';
  document.getElementById('btn-check').disabled = true;
  document.getElementById('update-log-section').style.display = 'block';
  document.getElementById('update-log').textContent = '';
  document.getElementById('update-reboot').style.display = 'none';
  try {
    const r = await fetch('/api/update/install', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'branch='+encodeURIComponent(branch)});
    if (!r.ok) { toast('Update failed',true); document.getElementById('update-progress').style.display='none'; document.getElementById('btn-check').disabled=false; return; }
    pollUpdateStatus();
  } catch(e) { toast('Error: '+e,true); document.getElementById('update-progress').style.display='none'; document.getElementById('btn-check').disabled=false; }
}

async function pollUpdateStatus() {
  if (updatePollTimer) clearTimeout(updatePollTimer);
  try {
    const [statusRes, logRes] = await Promise.all([fetch('/api/update/status'),fetch('/api/update/log')]);
    const d = await statusRes.json();
    const pct = d.progress||0, stage = d.stage||'idle';
    document.getElementById('update-bar').style.width = pct+'%';
    document.getElementById('update-pct-text').textContent = pct+'%';
    document.getElementById('update-stage-text').textContent = STAGE_LABELS[stage]||stage;
    try { const ld = await logRes.json(); if (ld.log) { const el = document.getElementById('update-log'); el.textContent = ld.log; if (el.scrollHeight-el.scrollTop-el.clientHeight<60) el.scrollTop=el.scrollHeight; } } catch(e) {}
    if (stage === 'error') {
      document.getElementById('update-bar').style.background = 'var(--danger)';
      document.getElementById('update-stage-text').textContent = 'Error: '+(d.error||'unknown');
      document.getElementById('btn-check').disabled = false;
      toast('Update failed',true);
      document.getElementById('update-log').style.display = 'block';
      document.getElementById('log-arrow').innerHTML = '&#9660;';
      return;
    }
    if (stage === 'done') {
      document.getElementById('update-bar').style.background = 'var(--accent)';
      document.getElementById('btn-check').disabled = false;
      document.getElementById('update-reboot').style.display = 'block';
      toast('Update installed \u2014 reboot to apply', false, 5000);
      return;
    }
    updatePollTimer = setTimeout(pollUpdateStatus, 1500);
  } catch(e) { updatePollTimer = setTimeout(pollUpdateStatus, 3000); }
}

function toggleUpdateLog() {
  const el = document.getElementById('update-log');
  const arrow = document.getElementById('log-arrow');
  if (el.style.display === 'none') { el.style.display='block'; arrow.innerHTML='&#9660;'; el.scrollTop=el.scrollHeight; }
  else { el.style.display='none'; arrow.innerHTML='&#9654;'; }
}

function copyUpdateLog() {
  const text = document.getElementById('update-log').textContent;
  if (!text) { toast('No log',true); return; }
  if (navigator.clipboard) { navigator.clipboard.writeText(text).then(()=>toast('Copied')); }
  else { const ta=document.createElement('textarea');ta.value=text;ta.style.position='fixed';ta.style.opacity='0';document.body.appendChild(ta);ta.select();document.execCommand('copy');document.body.removeChild(ta);toast('Copied'); }
}

async function downloadBackup() {
  try {
    const r = await fetch('/api/backup');
    const blob = await r.blob();
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href=url;a.download='dubsiren-backup.json';a.click();
    URL.revokeObjectURL(url);
    toast('Backup downloaded');
  } catch(e) { toast('Backup failed',true); }
}

async function restoreBackup() {
  const file = document.getElementById('restore-file').files[0];
  if (!file) { toast('Select a file',true); return; }
  const data = await file.text();
  try {
    const r = await fetch('/api/restore', {method:'POST',headers:{'Content-Type':'application/json'},body:data});
    if (r.ok) { toast('Restored'); loadPresets(); loadOptions(); } else toast('Restore failed',true);
  } catch(e) { toast('Error: '+e,true); }
}

async function exitAP() {
  if (!confirm('Return to siren mode?')) return;
  try {
    await fetch('/api/exit', {method:'POST'});
    toast('Returning to siren mode...');
    setTimeout(() => { document.body.innerHTML = '<div class="exit-screen"><h2>Siren Mode Active</h2><p>You can disconnect from this network.</p></div>'; }, 1000);
  } catch(e) { toast('Error: '+e,true); }
}

// Theme
const DARK_THEMES = [
  {id:'midnight',name:'Midnight',colors:['#0a0a0a','#ff6600','#00ff88','#ccc']},
  {id:'deep-dub',name:'Deep Dub',colors:['#05080f','#00ccff','#00ff88','#8899bb']},
  {id:'reggae',name:'Reggae',colors:['#0a0f06','#ffcc00','#00ff44','#b0c890']},
  {id:'steppers',name:'Steppers',colors:['#0f0808','#ff2222','#ff4444','#cc9999']},
  {id:'roots',name:'Roots',colors:['#0e0a06','#cc9933','#66cc44','#aa9070']},
  {id:'irie',name:'Irie',colors:['#0a060e','#bb44ff','#44ff88','#9980bb']},
  {id:'kingston',name:'Kingston',colors:['#08080c','#ff9900','#00ff66','#9999aa']},
  {id:'dubplate',name:'Dubplate',colors:['#0c0c08','#e0c040','#88ee44','#aa9']},
  {id:'soundsystem',name:'Sound System',colors:['#060a0a','#00ee99','#00ffaa','#80aaa0']},
  {id:'heavyweight',name:'Heavyweight',colors:['#080608','#ff4488','#44ffaa','#aa88aa']},
];
const LIGHT_THEMES = [
  {id:'silver',name:'Silver',colors:['#f0f0f0','#ff6600','#00cc66','#444']},
  {id:'concrete',name:'Concrete',colors:['#e8e4e0','#2a6b4f','#2a9b5f','#4a4640']},
  {id:'driftwood',name:'Driftwood',colors:['#ece6dc','#b87333','#55aa44','#5a5040']},
  {id:'overcast',name:'Overcast',colors:['#e4e8ec','#4477cc','#33aa66','#4a5060']},
  {id:'bleached',name:'Bleached',colors:['#f4f0e8','#cc4400','#44bb55','#605848']},
  {id:'studio',name:'Studio',colors:['#eaeaea','#8833cc','#33cc77','#505050']},
];
const ALL_THEMES = DARK_THEMES.concat(LIGHT_THEMES);
let savedTheme = null;

function renderThemeSwatches(container, themes, cur) {
  document.getElementById(container).innerHTML = themes.map(t =>
    '<div class="theme-swatch'+(t.id===cur?' active':'')+'" onclick="previewTheme(\''+t.id+'\')">'+
      '<div class="swatch-bar">'+t.colors.map(c=>'<span style="background:'+c+'"></span>').join('')+'</div>'+
      '<div class="swatch-label">'+t.name+'</div></div>'
  ).join('');
}

function renderThemeGrid() {
  const cur = document.documentElement.getAttribute('data-theme')||'midnight';
  renderThemeSwatches('theme-grid-dark', DARK_THEMES, cur);
  renderThemeSwatches('theme-grid-light', LIGHT_THEMES, cur);
  const changed = savedTheme && cur !== savedTheme;
  document.getElementById('btn-save-theme').style.display = changed ? 'block' : 'none';
}

function previewTheme(id) {
  document.documentElement.setAttribute('data-theme', id);
  renderThemeGrid();
}

async function saveTheme() {
  const id = document.documentElement.getAttribute('data-theme')||'midnight';
  savedTheme = id;
  try { localStorage.setItem('dubsiren-theme', id); } catch(e) {}
  renderThemeGrid();
  try {
    await fetch('/api/siren/options', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'theme='+id});
  } catch(e) {}
  toast('Theme saved');
}

// Apply saved theme — try localStorage first for instant render, then sync from server
(function(){try{const s=localStorage.getItem('dubsiren-theme');savedTheme=s||'midnight';document.documentElement.setAttribute('data-theme',savedTheme);}catch(e){savedTheme='midnight';document.documentElement.setAttribute('data-theme','midnight');}})();

// Init
loadPresets();
loadOptions();
renderThemeGrid();
loadDspState();
startHeartbeat();

// Interaction guard: prevent live poll from overwriting slider being dragged
document.querySelectorAll('#live input[type=range]').forEach(el => {
  const name = el.id.replace('dsp-', '');
  el.addEventListener('pointerdown', () => activeDragging.add(name));
  const up = () => setTimeout(() => activeDragging.delete(name), 300);
  el.addEventListener('pointerup', up);
  el.addEventListener('pointercancel', up);
});

// Pause live poll when browser tab is hidden
document.addEventListener('visibilitychange', () => {
  if (document.hidden) { stopLivePoll(); stopPresetPoll(); }
  else if (document.getElementById('live').classList.contains('active')) startLivePoll();
  else if (document.getElementById('presets').classList.contains('active')) startPresetPoll();
});
</script>
</body>
</html>
)HTML";

} // namespace web_ui
