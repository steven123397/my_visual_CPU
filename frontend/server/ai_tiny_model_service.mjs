import { execFile } from 'node:child_process';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { promisify } from 'node:util';

const execFileAsync = promisify(execFile);

const DYNAMIC_TINY_MODEL_PRESETS = Object.freeze({
  balanced: Object.freeze({
    label: 'Balanced activations',
    rows: Object.freeze([
      Object.freeze([0x3c00, 0xc000, 0x4200]),
      Object.freeze([0x3800, 0x4000, 0xbc00]),
    ]),
    expected: Object.freeze([2.5, 5.5]),
  }),
  negative_clamp: Object.freeze({
    label: 'ReLU clamp path',
    rows: Object.freeze([
      Object.freeze([0xbc00, 0xbc00, 0xbc00]),
      Object.freeze([0x3c00, 0xc000, 0x4200]),
    ]),
    expected: Object.freeze([0.0, 2.5]),
  }),
});

const DYNAMIC_GEMM_RUNTIME_SHAPES = Object.freeze({
  two_rows_identity_tail: Object.freeze({
    label: '2x8 -> 2x4 reference batch',
    rows: Object.freeze([
      Object.freeze([1, 2, 3, 4, 5, 6, 7, 8]),
      Object.freeze([-1, 0, 1, 2, 3, 4, 5, 6]),
    ]),
    expected: Object.freeze([1, 2, 3, 8, -1, 0, 1, 6]),
  }),
  single_row_identity_head: Object.freeze({
    label: '1x8 -> 1x4 single-row slice',
    rows: Object.freeze([
      Object.freeze([1, 2, 3, 4, 5, 6, 7, 8]),
    ]),
    expected: Object.freeze([1, 2, 3, 8]),
  }),
});

const DYNAMIC_CNN_RUNTIME_SHAPES = Object.freeze({
  compact_2x2: Object.freeze({
    label: '3x3 -> 2x2 compact path',
    inputShape: Object.freeze([3, 3]),
    inputRows: Object.freeze([
      Object.freeze([1, -2, 3]),
      Object.freeze([-4, 5, -6]),
      Object.freeze([7, -8, 9]),
    ]),
    outputShape: Object.freeze([2]),
    expected: Object.freeze([15, 31]),
  }),
  full_3x3: Object.freeze({
    label: '4x4 -> 3x3 full path',
    inputShape: Object.freeze([4, 4]),
    inputRows: Object.freeze([
      Object.freeze([1, -2, 3, -4]),
      Object.freeze([5, -6, 7, -8]),
      Object.freeze([9, -10, 11, -12]),
      Object.freeze([13, -14, 15, -16]),
    ]),
    outputShape: Object.freeze([3]),
    expected: Object.freeze([0, 78, 0]),
  }),
});

const TINY_ATTENTION_PRESETS = Object.freeze({
  uniform_query: Object.freeze({
    label: 'Uniform value mix',
    valueVector: Object.freeze([1.0, 3.0]),
    expected: Object.freeze([2.0]),
  }),
  biased_query: Object.freeze({
    label: 'Higher value mix',
    valueVector: Object.freeze([2.0, 6.0]),
    expected: Object.freeze([4.0]),
  }),
});

function cloneJson(value) {
  return JSON.parse(JSON.stringify(value));
}

function httpError(statusCode, message) {
  const error = new Error(message);
  error.statusCode = statusCode;
  return error;
}

function assertPlainObject(value) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    throw httpError(400, 'request body must be a JSON object');
  }
}

function writeU8(buffer, offset, value) {
  buffer.writeUInt8(value, offset);
}

function writeU16(buffer, offset, value) {
  buffer.writeUInt16LE(value, offset);
}

function writeU32(buffer, offset, value) {
  buffer.writeUInt32LE(value, offset);
}

function runtimeShapeRecord(tensorIndex, rank, dims) {
  const buffer = Buffer.alloc(20);
  writeU16(buffer, 0, tensorIndex);
  writeU8(buffer, 2, rank);
  writeU8(buffer, 3, 0);
  for (let index = 0; index < 4; index += 1) {
    writeU32(buffer, 4 + index * 4, dims[index] ?? 0);
  }
  return buffer;
}

function buildDynamicTinyModelRuntimeShapeTable(batch) {
  return Buffer.concat([
    runtimeShapeRecord(0, 2, [batch, 3, 0, 0]),
    runtimeShapeRecord(2, 2, [batch, 2, 0, 0]),
    runtimeShapeRecord(3, 2, [batch, 2, 0, 0]),
    runtimeShapeRecord(4, 2, [batch, 1, 0, 0]),
  ]);
}

function buildDynamicGemmRuntimeShapeTable(rows) {
  return Buffer.concat([
    runtimeShapeRecord(0, 2, [rows, 8, 0, 0]),
    runtimeShapeRecord(2, 2, [rows, 4, 0, 0]),
  ]);
}

function buildDynamicCnnRuntimeShapeTable(inputRows, inputCols) {
  const reducedRows = inputRows - 1;
  const reducedCols = inputCols - 1;
  return Buffer.concat([
    runtimeShapeRecord(0, 2, [inputRows, inputCols, 0, 0]),
    runtimeShapeRecord(2, 2, [reducedRows, reducedCols, 0, 0]),
    runtimeShapeRecord(3, 2, [reducedRows, reducedCols, 0, 0]),
    runtimeShapeRecord(4, 2, [reducedCols, reducedRows, 0, 0]),
    runtimeShapeRecord(5, 1, [reducedRows, 0, 0, 0]),
  ]);
}

function buildU16Rows(rows) {
  const flat = rows.flat();
  const buffer = Buffer.alloc(flat.length * 2);
  flat.forEach((value, index) => {
    buffer.writeUInt16LE(value, index * 2);
  });
  return buffer;
}

function buildInt8Rows(rows) {
  const flat = rows.flat();
  const buffer = Buffer.alloc(flat.length);
  flat.forEach((value, index) => {
    buffer.writeInt8(value, index);
  });
  return buffer;
}

function buildFp32Values(values) {
  const buffer = Buffer.alloc(values.length * 4);
  values.forEach((value, index) => {
    buffer.writeFloatLE(value, index * 4);
  });
  return buffer;
}

function buildI32Values(values) {
  const buffer = Buffer.alloc(values.length * 4);
  values.forEach((value, index) => {
    buffer.writeInt32LE(value, index * 4);
  });
  return buffer;
}

function readTypedValues(buffer, dtype) {
  const values = [];
  for (let offset = 0; offset + 4 <= buffer.length; offset += 4) {
    if (dtype === 'fp32') {
      values.push(Number(buffer.readFloatLE(offset).toFixed(6)));
      continue;
    }
    if (dtype === 'int32') {
      values.push(buffer.readInt32LE(offset));
      continue;
    }
    throw new Error(`unsupported output dtype: ${dtype}`);
  }
  return values;
}

function parseValue(value) {
  if (/^-?\d+$/.test(value)) {
    return Number(value);
  }
  if (/^-?\d+\.\d+$/.test(value)) {
    return Number(value);
  }
  return value;
}

function parseKeyValueLine(line) {
  const tokens = line.trim().split(/\s+/);
  const record = {};
  for (const token of tokens.slice(1)) {
    const eq = token.indexOf('=');
    if (eq <= 0) {
      continue;
    }
    record[token.slice(0, eq)] = parseValue(token.slice(eq + 1));
  }
  return record;
}

function pickNumber(record, key) {
  const value = record[key];
  return typeof value === 'number' && Number.isFinite(value) ? value : 0;
}

function parseProfileStdout(stdout) {
  const lines = stdout.split(/\r?\n/);
  const profileLine = lines.find((line) => line.startsWith('ai_profile '));
  const aggregateLine = lines.find((line) => line.startsWith('ai_profile_aggregate '));
  const opLines = lines.filter((line) => line.startsWith('ai_profile_op '));
  if (!profileLine || !aggregateLine) {
    throw new Error('AI profile output is missing required summary lines');
  }

  const profile = parseKeyValueLine(profileLine);
  const aggregate = parseKeyValueLine(aggregateLine);
  return {
    profile: {
      progress: String(profile.progress ?? 'unknown'),
      shapeMode: String(profile.shape_mode ?? 'unknown'),
      runtimeShapes: String(profile.runtime_shapes ?? 'none'),
      bytesMoved: pickNumber(profile, 'bytes_moved'),
      retiredOps: pickNumber(profile, 'retired_ops'),
      deviceCycles: pickNumber(profile, 'device_cycles'),
      dmaCycles: pickNumber(profile, 'dma_cycles'),
      computeCycles: pickNumber(profile, 'compute_cycles'),
      stallCycles: pickNumber(profile, 'stall_cycles'),
      busyCycles: pickNumber(profile, 'busy_cycles'),
      queueCycles: pickNumber(profile, 'queue_cycles'),
      completionCycles: pickNumber(profile, 'completion_cycles'),
      utilization: pickNumber(profile, 'utilization'),
      effectiveOpsPerCycle: pickNumber(profile, 'effective_ops_per_cycle'),
    },
    aggregate: {
      tileCount: pickNumber(aggregate, 'tile_count'),
      scratchpadPeakBytes: pickNumber(aggregate, 'scratchpad_peak_bytes'),
      opCount: pickNumber(aggregate, 'op_count'),
    },
    ops: opLines.map((line) => {
      const op = parseKeyValueLine(line);
      return {
        opIndex: pickNumber(op, 'op_index'),
        opcode: String(op.opcode ?? 'unknown'),
        retiredOps: pickNumber(op, 'retired_ops'),
        computeCycles: pickNumber(op, 'compute_cycles'),
        stallCycles: pickNumber(op, 'stall_cycles'),
        tileCount: pickNumber(op, 'tile_count'),
      };
    }),
  };
}

async function packWorkload({ repoRoot, outDir, workloadId }) {
  const myCpuRoot = path.join(repoRoot, 'myCPU');
  const packer = path.join(myCpuRoot, 'workloads', 'ai_proto', 'pack_graph.py');
  await execFileAsync('python3', [
    packer,
    '--workload',
    workloadId,
    '--out-dir',
    outDir,
  ], {
    cwd: myCpuRoot,
    timeout: 10000,
    maxBuffer: 1024 * 1024,
  });
}

async function prepareDynamicTinyModel({ outDir, parameters, repoRoot }) {
  const { batch, inputPreset } = parameters;
  await packWorkload({
    repoRoot,
    outDir,
    workloadId: 'dynamic_tiny_model',
  });

  const preset = DYNAMIC_TINY_MODEL_PRESETS[inputPreset];
  const rows = preset.rows.slice(0, batch);
  const expected = preset.expected.slice(0, batch);
  await fs.writeFile(
    path.join(outDir, 'dynamic_tiny_model.runtime_shape.bin'),
    buildDynamicTinyModelRuntimeShapeTable(batch),
  );
  await fs.writeFile(
    path.join(outDir, 'dynamic_tiny_model.input0.bin'),
    buildU16Rows(rows),
  );
  await fs.writeFile(
    path.join(outDir, 'dynamic_tiny_model.output0.expected.bin'),
    buildFp32Values(expected),
  );
  return {
    manifestPath: path.join(outDir, 'dynamic_tiny_model.manifest'),
    actualPath: path.join(outDir, 'dynamic_tiny_model.output0.actual.bin'),
    expected,
    output: {
      dtype: 'fp32',
      shape: [batch, 1],
    },
  };
}

async function prepareDynamicGemm({ outDir, parameters, repoRoot }) {
  const { runtimeShape } = parameters;
  await packWorkload({
    repoRoot,
    outDir,
    workloadId: 'dynamic_gemm',
  });

  const preset = DYNAMIC_GEMM_RUNTIME_SHAPES[runtimeShape];
  const rows = preset.rows.length;
  await fs.writeFile(
    path.join(outDir, 'dynamic_gemm.runtime_shape.bin'),
    buildDynamicGemmRuntimeShapeTable(rows),
  );
  await fs.writeFile(
    path.join(outDir, 'dynamic_gemm.input0.bin'),
    buildInt8Rows(preset.rows),
  );
  await fs.writeFile(
    path.join(outDir, 'dynamic_gemm.output0.expected.bin'),
    buildI32Values(preset.expected),
  );
  return {
    manifestPath: path.join(outDir, 'dynamic_gemm.manifest'),
    actualPath: path.join(outDir, 'dynamic_gemm.output0.actual.bin'),
    expected: [...preset.expected],
    output: {
      dtype: 'int32',
      shape: [rows, 4],
    },
  };
}

async function prepareTinyAttentionStatic({ outDir, parameters, repoRoot }) {
  const { inputPreset } = parameters;
  await packWorkload({
    repoRoot,
    outDir,
    workloadId: 'tiny_attention_static',
  });

  const preset = TINY_ATTENTION_PRESETS[inputPreset];
  await fs.writeFile(
    path.join(outDir, 'tiny_attention_static.input2.bin'),
    buildFp32Values(preset.valueVector),
  );
  await fs.writeFile(
    path.join(outDir, 'tiny_attention_static.output0.expected.bin'),
    buildFp32Values(preset.expected),
  );
  return {
    manifestPath: path.join(outDir, 'tiny_attention_static.manifest'),
    actualPath: path.join(outDir, 'tiny_attention_static.output0.actual.bin'),
    expected: [...preset.expected],
    output: {
      dtype: 'fp32',
      shape: [1, 1],
    },
  };
}

async function prepareDynamicCnn({ outDir, parameters, repoRoot }) {
  const { runtimeShape } = parameters;
  await packWorkload({
    repoRoot,
    outDir,
    workloadId: 'dynamic_cnn',
  });

  const preset = DYNAMIC_CNN_RUNTIME_SHAPES[runtimeShape];
  const [inputRows, inputCols] = preset.inputShape;
  await fs.writeFile(
    path.join(outDir, 'dynamic_cnn.runtime_shape.bin'),
    buildDynamicCnnRuntimeShapeTable(inputRows, inputCols),
  );
  await fs.writeFile(
    path.join(outDir, 'dynamic_cnn.input0.bin'),
    buildInt8Rows(preset.inputRows),
  );
  await fs.writeFile(
    path.join(outDir, 'dynamic_cnn.output0.expected.bin'),
    buildI32Values(preset.expected),
  );
  return {
    manifestPath: path.join(outDir, 'dynamic_cnn.manifest'),
    actualPath: path.join(outDir, 'dynamic_cnn.output0.actual.bin'),
    expected: [...preset.expected],
    output: {
      dtype: 'int32',
      shape: [...preset.outputShape],
    },
  };
}

const TEMPLATE_DEFINITIONS = Object.freeze([
  Object.freeze({
    id: 'dynamic_tiny_model',
    title: 'Parameterized Tiny Model',
    summary: 'Server-generated bounded dynamic tiny model: fp16 GEMM -> fp32 ReLU -> fp32 max-pool.',
    shapeMode: 'dynamic_bounded',
    dtype: 'fp16/fp32',
    opChain: Object.freeze(['gemm', 'eltwise_relu', 'pool_max']),
    parameters: Object.freeze({
      batch: Object.freeze({
        label: 'Batch',
        default: 1,
        choices: Object.freeze([1, 2]),
      }),
      inputPreset: Object.freeze({
        label: 'Input preset',
        default: 'balanced',
        choices: Object.freeze(Object.keys(DYNAMIC_TINY_MODEL_PRESETS)),
        choiceLabels: Object.freeze(
          Object.fromEntries(
            Object.entries(DYNAMIC_TINY_MODEL_PRESETS).map(([key, value]) => [key, value.label]),
          ),
        ),
      }),
    }),
    boundary: Object.freeze({
      allowsCustomGraph: false,
      allowsModelUpload: false,
      maxBatch: 2,
      serverGeneratedGraph: true,
    }),
    demo: Object.freeze({
      expectedMarker: 'balanced returns 2.5, 5.5; negative_clamp returns 0, 2.5; output matches expected fp32 values.',
      proves: Object.freeze([
        'Server regenerates the bounded graph package, runtime shape table, inputs and expected output on every run.',
        'The profile path stays aligned with mycpu --ai-profile-manifest instead of a browser-side graph interpreter.',
      ]),
      boundaries: Object.freeze([
        'No custom graph upload or arbitrary model import.',
        'Only approved batch and input preset values are allowed.',
      ]),
    }),
    prepare: prepareDynamicTinyModel,
  }),
  Object.freeze({
    id: 'dynamic_gemm',
    title: 'Dynamic GEMM Profile',
    summary: 'Server-generated bounded dynamic GEMM: approved runtime shapes only, with int8 input and int32 output.',
    shapeMode: 'dynamic_bounded',
    dtype: 'int8/int32',
    opChain: Object.freeze(['gemm']),
    parameters: Object.freeze({
      runtimeShape: Object.freeze({
        label: 'Runtime shape',
        default: 'two_rows_identity_tail',
        choices: Object.freeze(Object.keys(DYNAMIC_GEMM_RUNTIME_SHAPES)),
        choiceLabels: Object.freeze(
          Object.fromEntries(
            Object.entries(DYNAMIC_GEMM_RUNTIME_SHAPES).map(([key, value]) => [key, value.label]),
          ),
        ),
      }),
    }),
    boundary: Object.freeze({
      allowsCustomGraph: false,
      allowsModelUpload: false,
      approvedRuntimeShapes: 2,
      serverGeneratedGraph: true,
    }),
    demo: Object.freeze({
      expectedMarker: 'single_row_identity_head returns 1, 2, 3, 8; two_rows_identity_tail returns the paired 2x4 identity slice.',
      proves: Object.freeze([
        'Runtime shape gating proves the bounded dynamic GEMM path without exposing arbitrary matrix sizes.',
        'The same host profile reports shape-dependent runtime tables and int32 accumulation output.',
      ]),
      boundaries: Object.freeze([
        'No arbitrary matrix sizes outside the whitelist.',
        'The template is still a server-generated micro profile, not a general GEMM authoring surface.',
      ]),
    }),
    prepare: prepareDynamicGemm,
  }),
  Object.freeze({
    id: 'dynamic_cnn',
    title: 'Dynamic CNN Profile',
    summary: 'Server-generated bounded dynamic CNN: conv2d -> ReLU -> transpose -> reduce on approved runtime shapes only.',
    shapeMode: 'dynamic_bounded',
    dtype: 'int8/int32',
    opChain: Object.freeze(['conv2d', 'eltwise_relu', 'layout_transpose', 'reduce_sum']),
    parameters: Object.freeze({
      runtimeShape: Object.freeze({
        label: 'Runtime shape',
        default: 'compact_2x2',
        choices: Object.freeze(Object.keys(DYNAMIC_CNN_RUNTIME_SHAPES)),
        choiceLabels: Object.freeze(
          Object.fromEntries(
            Object.entries(DYNAMIC_CNN_RUNTIME_SHAPES).map(([key, value]) => [key, value.label]),
          ),
        ),
      }),
    }),
    boundary: Object.freeze({
      allowsCustomGraph: false,
      allowsModelUpload: false,
      approvedRuntimeShapes: 2,
      serverGeneratedGraph: true,
    }),
    demo: Object.freeze({
      expectedMarker: 'compact_2x2 returns 15, 31; full_3x3 returns 0, 78, 0.',
      proves: Object.freeze([
        'conv2d -> relu -> transpose -> reduce stays observable under bounded runtime shapes.',
        'The profile exposes how runtime shape changes flow through conv output, transposed layout and reduced output.',
      ]),
      boundaries: Object.freeze([
        'No free-form CNN graph authoring.',
        'Only the approved 3x3 and 4x4 input shapes are accepted.',
      ]),
    }),
    prepare: prepareDynamicCnn,
  }),
  Object.freeze({
    id: 'tiny_attention_static',
    title: 'Tiny Attention Static',
    summary: 'Server-generated static attention-like graph: fp16 GEMM -> fp32 softmax -> fp32 GEMM.',
    shapeMode: 'static',
    dtype: 'fp16/fp32',
    opChain: Object.freeze(['gemm', 'softmax', 'gemm']),
    parameters: Object.freeze({
      inputPreset: Object.freeze({
        label: 'Input preset',
        default: 'uniform_query',
        choices: Object.freeze(Object.keys(TINY_ATTENTION_PRESETS)),
        choiceLabels: Object.freeze(
          Object.fromEntries(
            Object.entries(TINY_ATTENTION_PRESETS).map(([key, value]) => [key, value.label]),
          ),
        ),
      }),
    }),
    boundary: Object.freeze({
      allowsCustomGraph: false,
      allowsModelUpload: false,
      serverGeneratedGraph: true,
      staticGraph: true,
    }),
    demo: Object.freeze({
      expectedMarker: 'uniform_query returns 2; biased_query returns 4.',
      proves: Object.freeze([
        'softmax remains visible as a fixed static graph profile with deterministic expected output.',
        'The whitelist can expose an attention-like chain without opening a general transformer runtime.',
      ]),
      boundaries: Object.freeze([
        'Not a general transformer runtime or model upload path.',
        'Only the approved value presets are accepted.',
      ]),
    }),
    prepare: prepareTinyAttentionStatic,
  }),
]);

const TEMPLATE_BY_ID = new Map(TEMPLATE_DEFINITIONS.map((template) => [template.id, template]));
const KNOWN_PARAMETER_KEYS = new Set([
  'template',
  ...TEMPLATE_DEFINITIONS.flatMap((template) => Object.keys(template.parameters ?? {})),
]);
const DEFAULT_TEMPLATE_ID = TEMPLATE_DEFINITIONS[0]?.id ?? 'dynamic_tiny_model';

function listChoices(parameter) {
  return Array.isArray(parameter?.choices) ? parameter.choices : [];
}

function publicTemplate(template) {
  return {
    id: template.id,
    title: template.title,
    summary: template.summary,
    shapeMode: template.shapeMode,
    dtype: template.dtype,
    opChain: template.opChain,
    parameters: template.parameters,
    boundary: template.boundary,
    demo: template.demo,
  };
}

function normalizeParameterValue(parameterName, definition, rawValue) {
  const choices = listChoices(definition);
  if (choices.length === 0) {
    return rawValue;
  }

  let value = rawValue;
  if (value == null) {
    value = definition.default ?? choices[0];
  }

  const numericChoices = choices.every((choice) => typeof choice === 'number');
  if (numericChoices) {
    value = Number(value);
  } else {
    value = String(value);
  }

  const normalized = choices.find((choice) => String(choice) === String(value));
  if (normalized == null) {
    throw httpError(400, `${parameterName} must be one of: ${choices.join(', ')}`);
  }
  return normalized;
}

function normalizeRunRequest(payload) {
  assertPlainObject(payload);

  const templateId = payload.template ?? DEFAULT_TEMPLATE_ID;
  const template = TEMPLATE_BY_ID.get(templateId);
  if (!template) {
    throw httpError(400, `unsupported AI tiny model template: ${templateId}`);
  }

  const allowedKeys = new Set(['template', ...Object.keys(template.parameters ?? {})]);
  for (const key of Object.keys(payload)) {
    if (allowedKeys.has(key)) {
      continue;
    }
    if (KNOWN_PARAMETER_KEYS.has(key)) {
      throw httpError(400, `unsupported AI tiny model parameter for ${templateId}: ${key}`);
    }
    throw httpError(400, `unsupported AI tiny model parameter: ${key}`);
  }

  const parameters = { template: template.id };
  for (const [name, definition] of Object.entries(template.parameters ?? {})) {
    parameters[name] = normalizeParameterValue(name, definition, payload[name]);
  }
  return parameters;
}

export function createAiTinyModelService({
  repoRoot,
  binaryPath = path.join(repoRoot, 'myCPU', 'mycpu'),
} = {}) {
  if (!repoRoot) {
    throw new Error('repoRoot is required');
  }

  return {
    templates() {
      return {
        templates: cloneJson(TEMPLATE_DEFINITIONS.map((template) => publicTemplate(template))),
      };
    },

    async run(payload = {}) {
      const parameters = normalizeRunRequest(payload);
      const template = TEMPLATE_BY_ID.get(parameters.template);
      const tempRoot = await fs.mkdtemp(path.join(os.tmpdir(), 'mycpu-ai-whitelist-profile-'));
      try {
        const prepared = await template.prepare({
          outDir: tempRoot,
          parameters,
          repoRoot,
        });
        const { stdout } = await execFileAsync(binaryPath, [
          '--ai-profile-manifest',
          prepared.manifestPath,
        ], {
          cwd: path.join(repoRoot, 'myCPU'),
          timeout: 15000,
          maxBuffer: 1024 * 1024,
        });
        const actual = readTypedValues(
          await fs.readFile(prepared.actualPath),
          prepared.output.dtype,
        );
        const parsed = parseProfileStdout(stdout);
        return {
          ok: true,
          template: template.id,
          title: template.title,
          parameters,
          output: {
            dtype: prepared.output.dtype,
            shape: prepared.output.shape,
            values: actual,
            expected: prepared.expected,
          },
          ...parsed,
          boundary: cloneJson(template.boundary),
        };
      } catch (error) {
        if (Number.isInteger(error?.statusCode)) {
          throw error;
        }
        const message = error?.stderr
          ? String(error.stderr).trim()
          : (error?.message ?? 'AI tiny model run failed');
        throw httpError(500, message || 'AI tiny model run failed');
      } finally {
        await fs.rm(tempRoot, { recursive: true, force: true });
      }
    },
  };
}
