import ConfigScreen from './components/ConfigScreen';
import MainScreen from './components/MainScreen';
import { useRoute } from './router';

export default function App() {
  const route = useRoute();

  // Keyed by route so each screen remounts and plays the .screen entrance animation
  return (
    <div className="screen" key={route}>
      {route === '/config' ? <ConfigScreen /> : <MainScreen />}
    </div>
  );
}
