// Client for the thermostat REST API (see main/rest_api.hpp).
// Add ?mock to the page URL to run against fake data without a device.

export type Mode = 'off' | 'manual' | 'schedule';

export interface ThermostatState {
  mode: Mode;
  setpoint: number;
  current_temp: number;
  humidity: number;
  pressure: string;
  heater_on: boolean;
}

export const SETPOINT_MIN = 50;
export const SETPOINT_MAX = 90;

export const MODES: Mode[] = ['off', 'manual', 'schedule'];

const useMock = new URLSearchParams(window.location.search).has('mock');

let mock: ThermostatState = {
  mode: 'manual',
  setpoint: 64,
  current_temp: 71,
  humidity: 39,
  pressure: '22.71',
  heater_on: false,
};

async function request<T>(path: string, init?: RequestInit): Promise<T> {
  const response = await fetch(path, {
    ...init,
    headers: init?.body ? { 'Content-Type': 'application/json' } : undefined,
  });
  if (!response.ok) {
    throw new Error(`${response.status} ${response.statusText}`);
  }
  return response.json() as Promise<T>;
}

export async function fetchThermostat(): Promise<ThermostatState> {
  if (useMock) {
    return mock;
  }
  return request<ThermostatState>('/api/thermostat');
}

export async function updateThermostat(
  patch: Partial<Pick<ThermostatState, 'mode' | 'setpoint'>>,
): Promise<ThermostatState> {
  if (useMock) {
    mock = { ...mock, ...patch };
    return mock;
  }
  return request<ThermostatState>('/api/thermostat', {
    method: 'PUT',
    body: JSON.stringify(patch),
  });
}

export interface Settings {
  temperature_hysteresis: number;
  inactivity_timeout_s: number;
  active_brightness: number;
  idle_brightness: number;
}

let mockSettings: Settings = {
  temperature_hysteresis: 2,
  inactivity_timeout_s: 30,
  active_brightness: 100,
  idle_brightness: 3,
};

export async function fetchSettings(): Promise<Settings> {
  if (useMock) {
    return mockSettings;
  }
  return request<Settings>('/api/settings');
}

export async function updateSettings(patch: Partial<Settings>): Promise<Settings> {
  if (useMock) {
    mockSettings = { ...mockSettings, ...patch };
    return mockSettings;
  }
  return request<Settings>('/api/settings', {
    method: 'PUT',
    body: JSON.stringify(patch),
  });
}
