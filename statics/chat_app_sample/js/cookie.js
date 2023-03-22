const set_cookie = (cname, cvalue, exdays) => {
  const d = new Date();
  d.setTime(d.getTime() + exdays * 24 * 60 * 60 * 1000);
  let expires = "expires=" + d.toUTCString();
  document.cookie = cname + "=" + cvalue + ";" + expires + ";path=/";
};

const delete_cookie = (cname) => {
    set_cookie(cname, "", -1);
}

const get_cookie = (cname) => {
  let name = cname + "=";
  let ret = null;
  document.cookie
    .split(";")
    .map((item) => item.trim())
    .forEach((item) => {
      if (item.startsWith(name)) {
        ret = item.split("=")[1];
      }
    });
  return ret;
};
