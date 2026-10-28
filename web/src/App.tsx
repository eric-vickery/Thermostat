import ConfigScreen from './components/ConfigScreen';
import MainScreen from './components/MainScreen';
import ScheduleScreen from './components/ScheduleScreen';
import { useRoute } from './router';

const SCREENS: Record<string, () => JSX.Element> = {
  '/config': ConfigScreen,
  '/schedule': ScheduleScreen,
};

export default function App() {
  const route = useRoute();
  const Screen = SCREENS[route] ?? MainScreen;

  // Keyed by route so each screen remounts and plays the .screen entrance animation
  return (
    <div className="screen" key={route}>
      <Screen />
    </div>
  );
}
