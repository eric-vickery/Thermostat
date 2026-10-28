import React from 'react';
import ReactDOM from 'react-dom/client';
import '@fontsource-variable/montserrat';
import './styles/theme.css';
import './styles/main-screen.css';
import './styles/config-screen.css';
import './styles/schedule-screen.css';
import App from './App';

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>,
);
