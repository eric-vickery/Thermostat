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
  remote_missing: boolean;
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
  remote_missing: false,
};

async function request<T>(path: string, init?: RequestInit): Promise<T> {
  const response = await fetch(path, {
    ...init,
    headers: init?.body ? { 'Content-Type': 'application/json' } : undefined,
  });
  if (!response.ok) {
    // The device describes errors as {"error":"message"}
    const body = await response.json().catch(() => null);
    throw new Error(body?.error ?? `${response.status} ${response.statusText}`);
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
  altitude_ft: number;
}

let mockSettings: Settings = {
  temperature_hysteresis: 2,
  inactivity_timeout_s: 30,
  active_brightness: 100,
  idle_brightness: 3,
  altitude_ft: 0,
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

export const DAY_NAMES = [
  'sunday',
  'monday',
  'tuesday',
  'wednesday',
  'thursday',
  'friday',
  'saturday',
] as const;

export type DayName = (typeof DAY_NAMES)[number];

export const MAX_EVENTS_PER_DAY = 6;

export interface ScheduleEvent {
  index: number;
  time: string; // "HH:MM"
  setpoint: number;
}

export interface DaySchedule {
  day: DayName;
  events: ScheduleEvent[];
}

// The mock follows the device's rules: events sorted by time, no duplicate times, 6 per day
let mockSchedule: DaySchedule[] = DAY_NAMES.map((day) => ({
  day,
  events: [] as ScheduleEvent[],
}));

function mockDay(day: DayName): DaySchedule {
  return mockSchedule[DAY_NAMES.indexOf(day)];
}

function mockResult(day: DayName): DaySchedule {
  const d = mockDay(day);
  d.events.sort((a, b) => a.time.localeCompare(b.time));
  d.events.forEach((e, i) => (e.index = i));
  return structuredClone(d);
}

export async function fetchSchedule(): Promise<DaySchedule[]> {
  if (useMock) {
    return DAY_NAMES.map(mockResult);
  }
  return (await request<{ days: DaySchedule[] }>('/api/schedule')).days;
}

export async function addScheduleEvent(
  day: DayName,
  time: string,
  setpoint: number,
): Promise<DaySchedule> {
  if (useMock) {
    const d = mockDay(day);
    if (d.events.length >= MAX_EVENTS_PER_DAY) {
      throw new Error('day already has the maximum number of events');
    }
    if (d.events.some((e) => e.time === time)) {
      throw new Error('an event already exists at that time');
    }
    d.events.push({ index: 0, time, setpoint });
    return mockResult(day);
  }
  return request<DaySchedule>(`/api/schedule/${day}/events`, {
    method: 'POST',
    body: JSON.stringify({ time, setpoint }),
  });
}

export async function removeScheduleEvent(day: DayName, index: number): Promise<DaySchedule> {
  if (useMock) {
    mockDay(day).events.splice(index, 1);
    return mockResult(day);
  }
  return request<DaySchedule>(`/api/schedule/${day}/events/${index}`, { method: 'DELETE' });
}

export async function clearScheduleDay(day: DayName): Promise<DaySchedule> {
  if (useMock) {
    mockDay(day).events = [];
    return mockResult(day);
  }
  return request<DaySchedule>(`/api/schedule/${day}`, { method: 'DELETE' });
}

// Copies the events of one day over another and returns the changed (target) day
export async function cloneScheduleDay(from: DayName, to: DayName): Promise<DaySchedule> {
  if (useMock) {
    mockDay(to).events = structuredClone(mockDay(from).events);
    return mockResult(to);
  }
  return request<DaySchedule>(`/api/schedule/${from}/clone`, {
    method: 'POST',
    body: JSON.stringify({ to }),
  });
}

// Edits are only in the device's memory until this writes them to its flash
export async function saveSchedule(): Promise<void> {
  if (useMock) {
    return;
  }
  await request<{ saved: boolean }>('/api/schedule/save', { method: 'POST', body: '{}' });
}
